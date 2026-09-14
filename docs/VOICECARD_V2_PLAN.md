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
| Limit (32,768 less the 1,024-byte bootloader) | 31,744 |
| **Free today** | **1,524** |

That 1,524 bytes is why this has to happen before anything else can.

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

## 4. The signal path question, worth settling early

The DAC is 12-bit. The engine is **8-bit**: `audio_data_type` is `uint8_t` and
the ISR does `sample * 16` to fill the 12-bit word. Four bits of the converter
are being thrown away, for a noise floor around 48 dB before the analog filter.

Moving the internal path to 12- or 16-bit is a genuine quality improvement that
needs **no hardware change**. It costs:

- RAM: the 128-sample ring buffer doubles, +128 bytes of the 976 free.
- CPU: every oscillator's inner loop widens from 8-bit to 16-bit math, which on
  an 8-bit AVR is roughly a doubling of the arithmetic in the hottest code.

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
Ambika already does. Three areas, in this order:

1. **Wavefolding** — cheapest, genuinely new, start here.
2. **One delay line, two payoffs** — plucked string and comb. New class of sound,
   and the RAM decision in §4 hangs off it.
3. **Band-limited ensemble** — only if the cycle budget turns out to allow it.

That is a bigger sonic change than a dozen half-working ports, and it fits.

---

## 6. Patch compatibility and the converter

`Patch` is 84 bytes, `PartData` 112, `MultiData` 56, all stored as RIFF chunks
with a structure-ID byte (1: Patch, 2: sequence, 4: MultiData, 5: PartData).

That structure-ID byte is the clean way through. **Give v2 patches a new
structure ID.** The controller then knows unambiguously which layout a file
holds, with no version guessing and no format sniffing.

Then convert **in firmware, on load**, not offline:

- The controller has ~8.9 KB of flash free, and an old-to-new waveform remap is
  a table of about 40 bytes plus a little code.
- The user experience is that old patches simply open. An offline tool means
  every user has to find and run it.
- Waveforms with no successor (the 16 wavetables, wavequence) map to the nearest
  survivor — most plausibly a detuned multi-saw or the polyBLEP wave — and the
  patch is marked as converted so saving it writes the v2 layout.

Carey's own estimate is that **95% of his patches use only the first few
oscillator types** — polyBLEP, triangle, noise, sine — so the remap will be a
no-op for almost everything real. Worth confirming by running a script over the
actual SD card library before designing the fallback mapping; `utils/` already
has `list_bank.py` and `ambika_program_as_text.py` to build on.

An offline batch converter in `utils/` is still worth having for bulk library
migration, but as a convenience, not as the mechanism.

---

## 7. Road ahead

**Phase 0 — close out v1.** Confirm the deferred-load build on hardware, tag
`v1.4`, cut the `v2-voicecard` branch. (§1)

**Phase 1 — measure.** Enable `TIMING_CODE`, record worst-case `ProcessBlock`
across existing algorithms, and establish the true free cycles per sample. Write
it into this document. Nothing else starts until this exists. (§3)

**Phase 2 — decide the signal path.** 8-bit, dithered 8-bit, or 12/16-bit, using
the Phase 1 number and the Karplus-Strong RAM question as the test case. (§4, §5)

**Phase 3 — remove the wavetables.** Delete the wavetable and bandlimited
renderers and their tables, reimplement triangle as polyBLEP, and confirm the
build drops to roughly 15 KB. No new features in this step — it should be a pure
subtraction, verifiable by size and by listening to the surviving waveforms.

**Phase 4 — new oscillator set.** One algorithm at a time, each with its cycle
cost measured and recorded before the next begins. Order: wavefolding, then the
delay-line family, then the band-limited ensemble if the budget allows.

**Phase 5 — patch conversion.** New structure ID, firmware remap on load, and a
pass over the real library to validate the mapping. (§6)

**Phase 6 — the AU/VST editor.** The SysEx name query works as of v1.4, so the
editor can already enumerate presets. A v2 patch layout means the editor's
parameter model changes too; do it after Phase 5 so it is written once.

**Later — new voice card hardware.** Explicitly out of scope here. Everything
above runs on the existing ATmega328 cards. Revisit hardware only once the
firmware has been pushed as far as it usefully goes, as recorded in the handover.

---

## Decisions

1. ~~**Vowel/formant**~~ — **keep.** Confirmed September 14, 2026. (§2)
2. **Signal path:** is a 12/16-bit path worth spending cycles on, or stay 8-bit
   and spend everything on oscillator types? The delay-line range halves if the
   path widens, so the plucked string probably decides it. (§4, §5)
3. **Anything in the current waveform set that must not be lost**, beyond
   polyBLEP saw/PWM/CSAW, triangle, sine, noise, vowel and the CZ family. (§6)
