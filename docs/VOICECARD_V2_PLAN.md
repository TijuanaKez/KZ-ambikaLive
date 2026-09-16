# Voice card v2 — architecture plan

Drafted September 14, 2026, at Carey's request, after the v1.3 stability release.
This is the first work that deliberately **breaks compatibility with original
Ambika**. Nothing here is implemented yet.

Every number below was measured from the current tree, not estimated. The one
number that matters most — the CPU budget — has *not* been measured, and that is
the first task.

---

## 1. Where the compatible line lives

`master` stays the Ambika-compatible v1 line. It is what the public downloads,
it keeps its release tags, and it can still take fixes after v2 begins.

v2 work happens on the **`v2-voicecard`** branch. When it is hardware-proven and
becomes the main line, the v1 history is preserved by its tags and by a
`v1-compat` branch cut at that moment.

The real safety net is the tags, which are already pushed:

- `v1.3` — the published stability release.
- `stable-controller-2026-09-14` — the DIAG3 hardware sign-off, same commit.
- `kz-local-preserved-2026-09-13`, `archive-published-2026-09-14` — pre-consolidation states.

Tag `v1.4` before starting v2, once the deferred-load build is confirmed on
hardware. That tag is the point we come back to.

---

## 2. What removing the wavetables actually buys

Measured from the linked ELF of the current voice card build (AVR GCC 9.5.0):

| | Bytes |
|---|---:|
| Voice card flash in use | **30,220** |
| Limit (0x7e00, where the bootloader is linked) | 32,256 |
| **Free today** | **2,036** |

That 2,036 bytes is why this has to happen before anything else can.

The limit was previously recorded as 31,744, assuming a 1 KB bootloader. The
authority is `voicecard/bootloader/makefile`, which links the bootloader at
`--section-start=.text=0x7e00` and states it must fit within 512 bytes, matching
HFUSE `0xde` (BOOTSZ = 11). The controller's 61,440 is correct by the same rule:
its bootloader is linked at `0xf000`.

### Why the GCC 9 build is 30,220 when the shipped v1.1 image is 26,160

Carey noticed the 4,060-byte gap before flashing. Investigated September 14, 2026;
it is accounted for, and none of it is unexplained bloat.

**The data is the same.** Probing the shipped `ambika_voicecard_v1.1.bin` for byte
sequences taken from our build's symbols: `wav_res_waves` (10,320),
`wav_res_wavetables` (288) and `lut_res_vca_linearization` (512) are all present
verbatim. Only `lut_res_oscillator_increments` (1,536) differs. Both images carry
roughly 19 KB of identical PROGMEM tables, so the whole difference is **code**:
about 11 KB in ours against about 7 KB in v1.1.

**Where the code difference comes from:**

| Cause | Bytes |
|---|---:|
| `-O2` instead of `-Os` | **1,476** (measured: 30,220 vs 28,744) |
| GCC 9 codegen and the MachFour refactor | ~2,600 (remainder) |

`voicecard/makefile` sets `OPTIMISATION_LEVEL = -O2`, deliberately overriding the
`-Os` that `avrlib/makefile.mk` defaults to. That is a reasonable choice for the
real-time half of the synth, and the controller does not do it.

**This is a live flash/CPU lever for v2, worth 1,476 bytes.** Do not simply flip
it: the voice card is the CPU-critical half and `-Os` may be slower in exactly
the inner loops that matter. Measure it in Phase 1 alongside the cycle budget —
if `-Os` costs nothing measurable at 39.2 kHz, it is 1.5 KB for free.

Note also that a size difference is not evidence of malfunction. The documented
hazard is a *silent* card from code generation, which size does not predict in
either direction. Only the hardware test settles that.

Reclaimable, by symbol:

| Table | Bytes | Used by |
|---|---:|---|
| `wav_res_waves` | 10,320 | `RenderInterpolatedWavetable`, `RenderWavequence` |
| `wav_res_bandlimited_*` (16 linked zones) | 4,112 | `RenderBandlimitedPwm`, `RenderSimpleWavetable` |
| `wav_res_wavetables` (index) | 288 | wavetable selection |
| Render code for the above | ~1,000 | four functions |
| **Total** | **~15,700** | |

With the current free space that is roughly **17 KB — over half the flash — for
new oscillator code.** Flash stops being the constraint.

**Keep** `wav_res_sine` (257 bytes). It is cheap and FM, vowel and the CZ family
all read it.

**Note on triangle:** `WAVEFORM_TRIANGLE` currently renders from the bandlimited
triangle zones, so "keep triangle" means *reimplement* it as an integrated
polyBLEP square. That is a small CPU cost and it is what frees the 4,112 bytes.

**Vowel/formant stays.** Carey confirmed, September 14, 2026. `wav_res_formant_*`
(512 bytes) and `RenderVowel` (604) are retained along with `WAVEFORM_VOWEL` and
`WAVEFORM_VOWEL_2`. It is a signature Shruthi/Ambika sound and is not a wavetable
in the sense being removed.

---

## 3. The real constraint is CPU, and it is not yet measured

Timer2, phase-correct PWM, prescaler 1, 8-bit: audio runs at
**20,000,000 / 510 = 39.2 kHz**, giving **510 cycles per sample**.

Out of those 510, the ISR already spends an unmeasured amount on the DAC SPI
writes and `voicecard_rx.Receive()`. Whatever is left covers two oscillators,
the sub oscillator, mixing and the control-rate work, rendered in blocks of 40
into a 128-byte ring buffer.

**`voicecard.cc` already has the instrumentation for this.** `#define
TIMING_CODE` at line 50 enables scope pins around `voice.ProcessBlock()` and an
interrupt counter. Turning it on and putting a scope on the pins gives the
budget directly.

> **Task 0 of this project: enable `TIMING_CODE`, measure the worst-case
> `ProcessBlock` duration across the existing algorithms, and write the number
> down.** Every proposal below is a guess until that exists. Do not design new
> oscillators against an unmeasured budget — this is the same discipline that
> caught the 23-byte stack margin on the controller.

SRAM is comparatively comfortable: `.data` 86 + `.bss` 986 = **1,072 of 2,048**,
leaving **976 bytes**. That is the figure that decides whether delay-line
algorithms are possible at all (see §5).

---

## 4a. Primary source: Emilie on the 8-bit engine

Recovered September 14, 2026 from the Wayback Machine, since both the Mutable
Instruments forum and its later mirror are gone. Thread *"Aliasing and noise"*,
January 2013, archived at
`web.archive.org/web/20130529002853/http://www.mutable-instruments.net/forum/discussion/2443/aliasing-and-noise`.

A user measured aliasing and noise **up to -40 dB** on the analog waveforms and
worked out that 8-bit gives only 48 dB of dynamic range. Emilie replied:

> "The internal precision for all audio rendering is 8-bit ; so there will always
> be quantization noise at -48dB."

and, on band-limiting:

> "Doing proper band-limited synthesis (minblep & co) is out of reach for the kind
> of cheap MCU used for Ambika. To generate a band-limited sawtooth or square,
> Ambika/Shruthi use wavetables. The higher the note you play, the simpler the
> waveform used... we use a simpler wavetable with 6 waveforms having different
> levels of harmonics (from sawtooth to sine); and crossfading is used... On the
> Shruthi/Ambika, a zone is 16 notes large. The main implication is that a
> trade-off has to be found for the point near the crossfade point. If you play
> conservatively so that no aliasing occurs at the crossfade point, you loose 30%
> of the higher harmonics at the non-crossfaded points. If you play aggressively
> so that the non-crossfaded points are maximally bright, you get very audible
> aliasing at the crossfade point. **The trade-off I have decided on is closer to
> the aggressive solution.**"

The 16-note zone matches `U8Swap4(note)` in `RenderSimpleWavetable` exactly.

**Three things follow, and the second reverses an earlier recommendation.**

1. There is **no evidence Emilie ever decided that 12-bit would be inaudible**.
   She states 8-bit as a constraint of the engine, not as a considered choice
   about audibility. The 8-bit engine is inherited from the Shruthi, whose output
   was natively 8-bit PWM; Ambika's technical notes say the 12-bit DAC was added
   to stop the filter self-oscillation interacting with the 39 kHz PWM carrier,
   not to gain resolution.
2. **Aliasing measured at -40 dB sits roughly 8 dB *above* the -48 dB
   quantization floor.** An earlier draft of this plan argued the opposite — that
   the 8-bit noise floor would mask any improvement in band-limiting. That was
   wrong, and Emilie's own trade-off statement says why: the zone crossfade was
   deliberately tuned bright, accepting audible aliasing. **Better oscillator
   maths is therefore the bigger lever, and the signal path the smaller one.**
   Reverse the priority in §4 and §6b accordingly.
3. **Emilie's "out of reach" assessment is dated.** polyBLEP is substantially
   cheaper than minBLEP, and YAM shipped working polyBLEP renderers for this
   exact hardware — they are in this tree, merely unreachable (§6c). The
   documented complaint in that thread is precisely the zone-crossfade artefact
   that shapes 1 and 2 still render through today.

She also ruled out variable-clock-rate synthesis, in detail, for reasons that
still hold: it cannot sum several oscillators into one DAC, the ATmega328p has
only one 16-bit timer, and DMA-to-SPI-DAC is impractical. Do not re-propose it.

---

## 4. The signal path question, worth settling early

The DAC is 12-bit. The engine is **8-bit**: `audio_data_type` is `uint8_t` and
the ISR does `sample * 16` to fill the 12-bit word. Four bits of the converter
are being thrown away, for a noise floor around 48 dB before the analog filter.

**Read §4a first — it downgrades this from the main lever to a secondary one.**
Measured aliasing (-40 dB) is above the quantization floor (-48 dB), so fixing
the band-limiting is worth more than widening the path. This section stands, but
it is no longer the first thing to spend cycles on.

Moving the internal path to 12- or 16-bit needs **no hardware change**. It costs:

- RAM: the 128-sample ring buffer doubles, +128 bytes of the 976 free.
- CPU: every oscillator's inner loop widens from 8-bit to 16-bit math. On an
  8-bit AVR that means multi-byte arithmetic throughout the hottest code, which
  is very likely what exhausts the 510-cycle budget. This, rather than any
  judgement about audibility, is the real reason the engine is 8-bit.

That CPU cost is almost certainly the deciding factor, and it trades directly
against how many new oscillator types fit. **Decide this before writing new
oscillators**, because retrofitting the width afterwards means rewriting all of
them. Measure first (§3), then choose.

A middle option worth considering: keep 8-bit rendering but dither before the
×16, which costs almost nothing and recovers some perceived resolution.

---

## 5. What BRAIDS actually offers us

BRAIDS is STM32F1, 72 MHz, 32-bit, with hardware multiply and divide, and ~96 KB
of flash. We have 20 MHz, 8-bit, 510 cycles per sample, and 31 KB. **No BRAIDS
code ports directly.** What ports is the *design*: which algorithms give the most
character per cycle, and how Emilie parameterised them down to one "timbre" knob.

Assessed against our budget, by feasibility:

**Already in Ambika — do not "port" these, we have them**

Check the existing set before adding anything. Several BRAIDS headline models
already exist here, in some cases more flexibly:

- *Hard sync.* Fully implemented and user-selectable: `OP_SYNC` as the mixer
  operator feeds osc 1's `sync_state` into osc 2's sync input
  (`voice.cc:441-446`, `update_phase_and_sync` in `oscillator.cc:769`). BRAIDS
  has two fixed sync models; we can sync **any** shape to any other.
- *Digital filter LP/PK/BP/HP on a pulse train.* This is the CZ phase-distortion
  family, ten variants of it.
- *Feedback FM.* `WAVEFORM_FM_FB`. The BRAIDS variants are parameter choices.
- *Detuned ensemble.* `RenderQuadSawPad` sums four detuned saws — but see below,
  because they are raw `phase >> 10` saws with no band limiting at all.
- *Vowel / VOSIM territory.* `RenderVowel` and `WAVEFORM_VOWEL_2`.
- *Filtered noise, bit crushing.* `RenderFilteredNoise`, and `voice.crush()`.

**Genuinely absent, and affordable**

- *Wavefolding* (triangle fold, sine fold). A fold is `abs`-based arithmetic or a
  small table — cheap — and there is nothing like it anywhere in Ambika. This is
  the clearest win on the list: new territory for very few cycles.
- *Ring modulation between the two oscillators.* One multiply. `OP_RING_MOD` may
  already cover this at the mixer; check before implementing.

**Genuinely absent, and worth the cycles — needs §3 first**

- *A delay line, and the family it unlocks.* At 39.2 kHz a delay line costs one
  byte per sample. With 976 bytes free the lowest usable note is about **40 Hz**
  at 8-bit — actually usable. Build it once and it gives **Karplus-Strong pluck**
  *and* **comb/saw-comb** textures, two whole classes of sound Ambika has never
  had, from one allocation. At 16-bit samples the range halves to 80 Hz, which is
  the strongest single argument against widening the signal path (§4).
- *Band-limited detuned ensemble.* Not new in kind — `RenderQuadSawPad` exists —
  but it sums four **raw, aliasing** saws. It works as a pad because the ensemble
  masks the aliasing. A polyBLEP version would be the "proper" supersaw and would
  hold together in the mid and upper registers where the current one falls apart.
  This is the most expensive item here: four polyBLEP saws per sample. Quite
  possibly unaffordable, which is exactly why §3 comes first.

**Not feasible on this hardware, do not attempt**

Additive/harmonics, modal and struck-bell resonators, granular clouds,
speech/LPC, chord engines, wave-map morphing. These all need either many
simultaneous oscillators, large tables, or per-sample division. Plaits is even
further out of reach than BRAIDS — it is a 32-bit floating-point engine.

**Concrete suggestion:** do not try to reproduce BRAIDS, and do not re-add what
Ambika already does. Priorities, revised by Carey on September 17, 2026 after
listening to Karplus-Strong examples and finding them unmusical:

1. **A better saw.** Carey's own patches are mostly saw -- the bank A survey puts
   `POLYBLEP_SAW` at 40.8% of all oscillators, more than double anything else --
   so this is where quality work pays back most. See §6b, and note that the
   budget for it is now known: **12% of CPU and 13 KB of flash are free.**
2. **A wider signal path.** 12-bit rather than 8-bit, into the 12-bit DAC that is
   already fitted. Previously ruled marginal on cycle cost; the measurement
   changes that. See §4 and the correction below.
3. **Wavefolding** — cheap, genuinely absent, and Carey likes the idea.
4. **Band-limited ensemble** — only as an *additional* shape. `QUAD_SAW_PAD`
   stays exactly as it is (§6).

**Karplus-Strong is dropped**, September 17, 2026: Carey listened to examples and
did not find it musical. The delay-line reasoning in this document is retained
only because a comb/allpass could still be reached the same way, and because the
RAM analysis was what originally constrained the signal-path decision.

### The signal-path question reopens

§4 concluded that widening to 12-bit was the smaller lever, and §4a downgraded it
further on the basis that measured aliasing (-40 dB) sat above the 8-bit
quantization floor (-48 dB), so the floor was not the limiting artefact.

**That measurement was taken on Emilie's wavetable saw, not on polyBLEP.** The
zone-crossfade she describes is a fundamentally noisier generator than the
polyBLEP renderer this tree now actually reaches (§6c -- before the dispatch fix,
shape 1 was rendering the bandlimited wavetable saw, so nobody here has heard the
polyBLEP saw until now).

So the ordering to establish, in this order:

1. **A/B the polyBLEP saw that now works** against the old wavetable saw. This
   costs nothing and may already be the improvement Carey is asking for. Until
   somebody listens, the rest is speculation.
2. **Measure where the aliasing now sits.** If polyBLEP has pushed it below
   -48 dB, then the 8-bit floor *is* the limiting artefact and widening the path
   becomes the single biggest available win.
3. Only then choose between higher-order polyBLEP, minBLEP with a residual table
   (13 KB of flash makes this affordable for the first time), and 2x oversampling
   with decimation (affordable at 12% load, but the decimation filter is the
   expensive part on an 8-bit core).

That is a bigger sonic change than a dozen half-working ports, and it fits.

---

## 5b. Prior art: joegiralt's "Carcosa" fork

<https://github.com/joegiralt/ambika> — tags through `v2.06`. Carey reports the
binaries would not load on his unit and it was buggy. Source read September 14,
2026; it is worth studying because he built **exactly** the two things proposed
above, and the ways it went wrong are instructive.

He restructured the voice card into four engines — `ENGINE_CLASSIC`,
`ENGINE_FM4OP`, `ENGINE_KS_PLUCK`, `ENGINE_WESTCOAST` — in `voicecard/karplus.h`,
`westcoast.h` and `fm4op.h`. The west coast engine is a proper Buchla-style
iterative wavefolder with bias, symmetry and 1-6 fold stages.

**Why the binaries likely failed, and what to do differently:**

1. **He kept the wavetables.** `WAV_RES_WAVES_SIZE 10320` is still in his
   `resources.h`, and his log contains *"Bump to Carcosa v2.04, fix flash
   overflow"*. He was adding three engines to a firmware that already had only
   2 KB free. We delete 15.7 KB **first** — that is the space he never had, and
   it is the main reason to keep Phase 3 ahead of Phase 4.

   His shipped image on Carey's SD card, `CARCOSA/VOICE1.BIN`, is 32,062 bytes.
   That **does** fit under 32,256, with 194 bytes to spare — an earlier note here
   claimed it was too large, which was wrong and came from the incorrect 31,744
   limit. So flash size alone does not explain why his binaries would not load;
   the CPU cost in (2) and the pitch clamp in (3) remain the better explanations.
   Worth remembering that 194 bytes is no margin at all for a firmware doing
   per-sample 32-bit arithmetic.
2. **`int32_t` arithmetic in per-sample loops.** 8 occurrences in `karplus.h`,
   4 in `fm4op.h`, 3 in `westcoast.h`, including
   `(static_cast<int32_t>(avg - lp_state_) * lp_cutoff) >> 8` inside the KS inner
   loop. A 32-bit multiply on an 8-bit AVR costs tens of cycles against a budget
   of 510 for *everything*. The YAM history has a matching commit, *"Fix for CPU
   overload with FM, qpwm and pad oscillators"* — this is a known failure mode on
   this hardware. Our rule: **no 32-bit arithmetic inside a per-sample loop.**
3. **His KS cannot play bass.** `kKarplusBufferSize = 192` with an `int16_t`
   delay line is 384 bytes, and 192 samples at 39.2 kHz puts the lowest
   fundamental at about **204 Hz — G#3**. `SetPitch` clamps
   `len` to the buffer size, so every note below that simply plays at the wrong
   pitch rather than failing audibly. His commit *"KS pitch tracking fix: fill
   entire buffer on trigger"* is him chasing the symptoms.
   **We have 976 bytes free.** At 8-bit samples that is about 40 Hz; at 16-bit,
   about 80 Hz. This is the concrete form of the §4 decision.
4. **He repurposes oscillator patch fields per engine** — `karplus.h` documents
   `osc[1].shape` becoming the excitation type, `osc[1].detune` becoming pluck
   position, and so on. That is wholesale patch incompatibility, and it is the
   opposite of Carey's requirement in §6. **Take his ideas, not his
   architecture.** New engines must be new shapes in the existing enum, with
   their extra parameters found somewhere that does not overload existing fields.

His fold routine and excitation types are good reference material and the licence
is GPL-3.0, the same as ours, so borrowing with attribution is fine.

---

## 6. Patch compatibility and the converter

`Patch` is 84 bytes, `PartData` 112, `MultiData` 56, all stored as RIFF chunks
with a structure-ID byte (1: Patch, 2: sequence, 4: MultiData, 5: PartData).

**Carey's rule, September 14, 2026: every surviving algorithmic oscillator keeps
its current enum position.** New algorithms are appended after them. This is the
single most important compatibility decision and it makes everything else easy.

Consequences:

- `WAVEFORM_NONE` through `WAVEFORM_VOWEL` keep their values. `QUAD_SAW_PAD`
  stays as-is — aliasing and all — because it is what Carey's patches reference.
  Any band-limited version is an **additional** shape, not a replacement.
- Only the 17 wavetable slots (`WAVEFORM_WAVETABLE_1`..`_16`, `WAVEFORM_WAVEQUENCE`)
  change meaning, plus `WAVEFORM_OLD_SAW` and `WAVEFORM_QUAD_PWM` if the
  bandlimited zones go.
- The enum has a hole where the wavetables were. **Leave the hole.** Do not
  compact it to save a byte of table; renumbering is exactly what breaks patches.
  Point the dead slots at a defined fallback and append new shapes after
  `WAVEFORM_VOWEL_2`.
- `WAVEFORM_TRIANGLE` and `WAVEFORM_SINE` keep their positions even though their
  implementation changes underneath (§2). The patch byte is unaffected.

Because only the dead slots move, **most patches need no conversion at all** —
consistent with Carey's estimate that 95% of his use only the first few types.
A patch referencing a wavetable renders the fallback instead. That can be handled
entirely in the voice card with no file rewriting, no new structure ID, and no
converter.

So the structure-ID and firmware-remap machinery described previously is **not
needed for the oscillator change**. Keep it in reserve for a later change that
actually alters the 84-byte `Patch` layout; at that point a new structure ID is
still the right mechanism, since the RIFF object chunk already carries one.

Still worth doing: run `utils/list_bank.py` over the real SD library to count
which shapes are actually referenced, before choosing what the dead slots fall
back to.

---

## 6c. The oscillator dispatch table was misaligned with the enum — FIXED

Found September 14, 2026 while surveying Carey's library, **confirmed on hardware
September 15, and fixed the same day** in the voice card v1.3 test build.

**Important correction.** An earlier revision of this section said the bug was
live in shipped firmware and that Carey's wavetable patches were therefore
already broken. That was wrong. The bug is in *this source tree's* voice card,
which had never been flashed to hardware until 2026-09-14. Carey's six cards run
the YAM-derived v1.1 image, whose table is correct. So:

- His patches, including the wavetable ones, render correctly on his instrument
  today. Removing the wavetables **will** change 15 of his 92 bank-A patches.
- The bug was a *regression* in any build made from this tree, not a
  pre-existing defect, and it had to be fixed before the GCC 9 voice card could
  be compared with v1.1 at all.

**The hardware confirmation.** Carey stepped through the shapes on the v1.2 test
card against a v1.1 card: saw, PWM and triangle *all sounded like a saw*, and the
two cards rejoined at sine. That is precisely what the table analysis predicted —
`POLYBLEP_SAW` reached the bandlimited saw (a saw by luck), `POLYBLEP_PWM` was
intercepted by a pulse-width special case that also called the saw renderer, and
`TRIANGLE` reached `RenderSimpleWavetable`, which emits saw zones for anything
that is not `SINE`. The tree had **no triangle renderer at all**.

`common/patch.h` carries the **YAM** enum: `WAVEFORM_POLYBLEP_SAW = 1`,
`POLYBLEP_PWM = 2`, `WAVETABLE_1 = 21`, `POLYBLEP_CSAW = 41`, `LAST = 43`
(verified by compiling, not by reading).

`voicecard/oscillator.h` carries **MachFour's** table and dispatch, written for
the *original* Ambika enum where `SAW = 1`, `SQUARE = 2`, the polyBLEP shapes sit
at 21-23 and the wavetables start at 24. Its 25-entry `fn_table` is in that order,
and its dispatch is

```cpp
uint8_t index = new_shape >= WAVEFORM_WAVETABLE_1 ? WAVEFORM_WAVETABLE_1 : new_shape;
```

which is correct in MachFour's world, where the wavetables are last. YAM's
equivalent has a second branch that MachFour's does not need and this tree lost:

```cpp
shape_ >= WAVEFORM_WAVETABLE_1
  ? (shape_ <= WAVEFORM_WAVEQUENCE ? WAVEFORM_WAVETABLE_1
                                   : shape_ - WAVEFORM_WAVEQUENCE + WAVEFORM_WAVETABLE_1)
  : shape_
```

**Consequences in the shipped firmware:**

| Patch byte | Enum name | What actually renders |
|---:|---|---|
| 1 | `POLYBLEP_SAW` | `RenderSimpleWavetable` — the *bandlimited* saw, not polyBLEP |
| 2 | `POLYBLEP_PWM` | `RenderBandlimitedPwm` (or SimpleWavetable at parameter 0) |
| 3, 4 | `TRIANGLE`, `SINE` | correct |
| 5-20 | CZ family, quad saw, FM, 8bitland, dirty PWM, noise, vowel | correct |
| 21-36 | `WAVETABLE_1..16` | `RenderPolyBlepWave`, inner switch unmatched |
| 37 | `WAVEQUENCE` | correct — explicitly special-cased |
| 38-40, 42 | `OLD_SAW`, `QUAD_PWM`, `FM_FB`, `VOWEL_2` | `RenderPolyBlepWave`, unmatched |
| 41 | `POLYBLEP_CSAW` | correct, by luck — the inner switch has a case for it |

For an unmatched shape the inner switch sets `next_sample = 0` for every sample
below MIDI note 108, leaving only the polyBLEP correction impulses at each phase
reset. **Those shapes should therefore sound near-silent or like a thin buzz**,
becoming a naive saw above note 107 where `use_simple_saw` takes over.

`RenderInterpolatedWavetable` at `fn_table[24]` is **unreachable**. It stays in
the binary at 406 bytes only because its address is in the table.

### Why this matters for the plan

1. **Removing the wavetables costs less than §2 assumed.** They already do not
   render. Nothing that currently works is lost.
2. **`wav_res_waves` is only reachable through `RenderWavequence`.** To free the
   full 10,320 bytes, wavequence has to go too — it is the one shape in that
   range that still works.
3. **Two of the three polyBLEP renderers are dead code** while the shapes that
   should reach them are served by the bandlimited tables instead. The "improved
   basic waveforms" work in §6b starts by *fixing the dispatch*, which may itself
   be an audible improvement for free — shapes 1 and 2 are 57% of Carey's
   oscillators.
4. **Fixing it changes how existing patches sound.** Shapes 1 and 2 become true
   polyBLEP (cleaner); shapes 21-42 start rendering as something audible. That is
   a deliberate decision, not a silent side effect — see below.

### Carey's library, bank A only (the only bank that matters)

92 programs, 184 oscillators, from `utils/survey_library.py`:

| Share | Shapes |
|---:|---|
| 57.1% | `POLYBLEP_SAW` (40.8%) + `POLYBLEP_PWM` (16.3%) — both currently bandlimited, not polyBLEP |
| 23.4% | sine, triangle, CSAW, quad saw pad, CZ saw, noise, dirty PWM, 8bitland, none |
| **10.9%** | wavetables and wavequence — 15 of 92 patches (16.3%) touch one |

His 95% estimate was close: **77 of 92 patches need nothing at all.** Of the 15
that do, all but the two wavequence ones are already broken by the dispatch bug.

Note that Carey built these patches on his own firmware, so the enum *names* in
the survey are not necessarily what he heard when he made them — the byte values
are the reliable part. The wavetable patches most likely date from before the
MachFour merge, when those slots still rendered.

### How it was fixed (voice card v1.3, 2026-09-15)

`fn_table` is reordered to match `common/patch.h`, and the dispatch gains YAM's
offset branch so every shape above the wavetable block indexes correctly. The
pulse-width special case is deleted. `RenderNewTriangle` is ported from the YAM
voicecard, and `RenderQuadSawPad` gains the `QUAD_PWM` branch for the same
reason. `RenderBandlimitedPwm` is left in the tree but is no longer referenced,
and the linker now drops it — the fixed build is 102 bytes *smaller*.

Two static_asserts pin the table length against `WAVEFORM_LAST` and the wavetable
block width, and pin the order of the first four shapes. This drift cannot recur
silently.

Out-of-range shape bytes are clamped to silence. Carey's card genuinely contains
patches with shape values 59, 96 and 229, which previously indexed past the end
of the table.

This does **not** touch the controller or v1.4, which are unaffected: the bug
lives entirely in the voice card's dispatch.

### What this means for the wavetable removal

Since the wavetables *do* work on Carey's cards, removing them is a real change
to 15 of his 92 bank-A patches, not a no-op. §2 and §6 are otherwise unaffected:
those 15 patches were always going to need attention, and the dead slots should
fall back to `POLYBLEP_SAW`.

---

## 6b. Better basic subtractive waveforms — Carey's stated priority

Carey's highest priority is not exotica but **better ordinary saw/square/
triangle/pulse, using better maths**. Current state: `RenderPolyBlepSaw`,
`RenderPolyBlepPwm` and `RenderPolyBlepCSaw` are first-order polyBLEP; triangle
and sine come from tables; `OLD_SAW` and `QUAD_PWM` use the bandlimited zones.

Candidates, cheapest first:

- **polyBLAMP for triangle.** PolyBLEP corrects a *value* discontinuity; polyBLAMP
  corrects a *slope* discontinuity, which is what a triangle has. It gives a
  properly band-limited triangle with no tables at all — replacing about 1.8 KB
  of bandlimited triangle zones with roughly a hundred bytes of code, at a cost
  comparable to the existing polyBLEP saw. Best effort-to-reward on this list,
  and it is what makes "keep triangle" cheap in §2.
- **EPTR (Efficient Polynomial Transition Regions).** A cheaper formulation than
  polyBLEP for saw and square at comparable quality — it modifies the waveform
  near the discontinuity instead of adding a correction, saving the separate
  residual computation. Worth prototyping head-to-head against the existing
  polyBLEP saw and keeping whichever measures better.
- **Second-order polyBLEP** for the existing saw/PWM where cycles allow. Better
  high-frequency behaviour, strictly more expensive.
- **DPW (differentiated parabolic wave).** Very cheap in principle — square the
  naive ramp, then differentiate — but differentiation amplifies quantisation
  noise, so it is precision-hungry. Only viable if the signal path widens (§4).

**Corrected by §4a.** An earlier draft argued that the 8-bit noise floor would
mask any band-limiting improvement, making the signal path the bigger lever. The
archived forum measurement says otherwise: aliasing at **-40 dB** against a
quantization floor at **-48 dB**. The aliasing is the louder defect, by about
8 dB, and Emilie tuned the zone crossfade bright on purpose.

So the order is: **fix the dispatch (§6c) first**, which moves shapes 1 and 2 —
57% of Carey's oscillators — off the zone-crossfade wavetables and onto the
polyBLEP renderers that are already written and already in the binary. That is
the single cheapest available improvement to the basic waveforms, and it targets
the exact artefact documented in that thread. Only then consider polyBLAMP, EPTR
and the signal path.

---

## 6d. Envelope curves — from the Carcosa fork

Carey's request, 2026-09-15. joegiralt's fork added envelope curve options and
the implementation is small enough to be worth taking. In his
`voicecard/envelope.h` the whole change is one branch in the render step:

```cpp
uint8_t step = linear_ ? (phase_ >> 8)
                       : InterpolateSample(wav_res_env_expo, phase_);
```

Our envelope already does the `InterpolateSample(wav_res_env_expo, phase)` half
(`voicecard/envelope.h:84`), so **linear is free** — the branch is what is
missing, not a table. His `ENVELOPE_CURVE_LOOP` and `LOOP_LINEAR` re-arm the
envelope at the end of its decay, turning it into an LFO-ish AD loop.

Cost: one branch per envelope render, plus state. Two bits per envelope selects
among four curves, so all three envelopes fit in **one byte**. The voice card has
978 bytes of SRAM free, so this is not a constraint.

**The important part is where that byte goes.** `Patch::Parameters` ends with
`uint8_t padding[6]` (`common/patch.h:352`). Putting the curve selection there
keeps `Patch` at 84 bytes, which matters more than it looks: `Storage::Load`
accepts an object chunk only when `expected_size == size.value - 4`, so **any
change to the size of `Patch` makes every existing patch file silently fail to
load**. A padding byte means old patches load unchanged and read zero, which must
therefore be `ENVELOPE_CURVE_EXPONENTIAL` — today's behaviour.

Same trick that housed the deferred-load delay in the settings record, and the
pattern for every future patch-level parameter until the padding runs out. Six
bytes left; spend them deliberately.

Worth considering beyond Carcosa's four: a per-envelope curve *amount* rather
than a discrete linear/exponential choice costs a byte per envelope instead of
two bits. Note §6b though — at 8-bit resolution the audible difference between
curve shapes is coarse.

---

## 7. Road ahead

**Phase 0 — close out v1. DONE.** v1.4 confirmed on hardware and released
2026-09-14. The GCC 9 voice card was then proven on hardware 2026-09-15 —
pitch, all shapes and the filter all correct — which retired the silent-card
risk that gated this whole plan. v2 work is proceeding on `master`; cut
`v1-compat` from the `v1.4` tag if the v1 line ever needs maintenance. (§1)

**Phase 1 — measure. DONE 2026-09-16. AUD = 5 of 40, so the voice card uses
about 12% of its cycle budget.** There is far more room for new oscillators than
the plan assumed. Karplus-Strong, wavefolding and a band-limited ensemble are all
affordable on that figure; measure each one here as it is written.

Controller stack, settled the same way: peak use is **240 bytes**. It read 334
while TIMER1 could re-enter itself without bound; an atomic re-entry guard in
`controller.cc` cut it to 240. With the MIDI output buffer at Emilie's 128 that
leaves 58 bytes of margin, so nothing else had to be sacrificed for it.

**Phase 1 detail — the instrument.**
Rather than a scope on the timing pins, each voice card now reports its audio
render headroom over SPI (`COMMAND_GET_AUDIO_HEADROOM`) and the diagnostic
controller shows all six on the `AUD` line. The value is the peak free space in
the 128-sample audio buffer at the moment a block started rendering: around 40 is
healthy, rising means falling behind, `kAudioStarved` (254) means it ran dry,
and `kAudioHeadroomUnsupported` (255) means that card predates the counter and
is displayed as `--`. Reading clears it.

This is permanent on purpose — Phase 4 requires each new algorithm's cost to be
recorded before the next is written, and this is what records it. Cost is 50
bytes of flash and 2 of SRAM on the voice card, diagnostic-only on the
controller.

**Outstanding: the baseline reading**, taken while playing something demanding,
and the per-algorithm readings. Write them into this document as they arrive.
`TIMING_CODE` remains available for cycle-accurate work if a scope is ever
convenient. (§3)

**Phase 2 — decide the signal path.** 8-bit, dithered 8-bit, or 12/16-bit, using
the Phase 1 number and the Karplus-Strong RAM question as the test case. (§4, §5)

**Phase 2b — fix the oscillator dispatch. DONE, 2026-09-15.** (§6c) Confirmed by
ear and fixed in voice card v1.3; the triangle renderer was ported from YAM.

**Phase 3 — remove the wavetables. DONE, 2026-09-15.** Voice card flash went
from 30,168 to **18,964** of 32,256, freeing **11,204 bytes**; free space went
from 2,088 to 13,292. `wav_res_waves`, `wav_res_wavetables`,
`RenderInterpolatedWavetable` and `RenderWavequence` are gone. The enum slots are
retained and remapped to `WAVEFORM_POLYBLEP_SAW` in `Oscillator::Render()`, so no
patch byte changes meaning and no conversion is needed.

Gated on the resource compiler, which needs numpy and had never been run in this
project. Verified first that regenerating `voicecard/resources.{cc,h}` unchanged
reproduces the committed files byte for byte — so generator edits are safe, and
adding tables for new algorithms later is now possible. Use a venv; do not
install numpy system-wide.

Still available: `WAVEFORM_OLD_SAW` is the only remaining reader of the
bandlimited zone tables (`wav_res_bandlimited_*`, 4,112 bytes). Retiring it and
reducing `RenderSimpleWavetable` to the sine path would free roughly **4.3 KB**
more. Never used in bank A.

**Phase 4 — new oscillator set.** One algorithm at a time, each with its cycle
cost measured and recorded before the next begins, and each appended after
`WAVEFORM_VOWEL_2` so no existing patch byte changes meaning (§6). Order:
improved basic waveforms (§6b), Karplus-Strong, wavefolding, then the
band-limited ensemble if the budget allows.

Hard rule throughout, learned from §5b: **no 32-bit arithmetic inside a
per-sample loop.**

**Phase 5 — patch fallbacks.** Much smaller than originally scoped, because
surviving shapes keep their enum positions. Decide what the dead wavetable slots
render, after counting what the real library actually references with
`utils/list_bank.py`. No file conversion and no new structure ID needed. (§6)

**Phase 6 — the AU/VST editor.** The SysEx name query works as of v1.4, so the
editor can already enumerate presets. A v2 patch layout means the editor's
parameter model changes too; do it after Phase 5 so it is written once.

**Later — new voice card hardware.** Explicitly out of scope here. Everything
above runs on the existing ATmega328 cards. Revisit hardware only once the
firmware has been pushed as far as it usefully goes, as recorded in the handover.

---

## Decisions

1. ~~**Vowel/formant**~~ — **keep.** Confirmed September 14, 2026. (§2)
2. ~~**Waveform set**~~ — **keep every algorithmic oscillator, at its current
   enum position**, `QUAD_SAW_PAD` included. New shapes append after
   `WAVEFORM_VOWEL_2`. Confirmed September 14, 2026. (§6)
3. **Signal path:** is a 12/16-bit path worth spending cycles on, or stay 8-bit
   and spend everything on oscillator types? Two things hang off it — the
   Karplus-Strong bass range (§5b) and whether better band-limiting maths is even
   audible (§6b). The cheap experiment in §6b settles it; run that early.
