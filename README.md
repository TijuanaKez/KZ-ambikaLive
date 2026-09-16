# KZ Ambika Live

A firmware fork for the **Mutable Instruments Ambika**, built on top of the
[YAM fork](https://github.com/bjoeri/ambika) by bjoeri, which is itself built on
Emilie Gillet's original.

Everything YAM adds is here too. What follows is what this fork adds on top.

**Latest release: [v1.5](../../releases/latest)** — controller only; voice cards
stay on v1.1 and do not need reflashing.

---

## Why you might want it

- **It builds on a modern toolchain.** AVR GCC 9.5.0 and current avr-libc, with
  no ancient CrossPack install required. `prog_char` and friends are gone.
- **It should not run out of memory.** Earlier builds — including stock YAM —
  could exhaust the controller's stack and corrupt memory. See *Stability* below.
- **Preset browsing is fast**, and a good deal of the interface is quicker to
  drive two-handed.

| | |
|---|---:|
| Controller flash | 54,262 / 61,440 |
| Controller static SRAM | 3,784 / 3,968 |
| Stack headroom | 312 bytes, against a measured peak of 254 |
| Voice card | unchanged, v1.1 |

---

## What this fork adds

### Interface

**ENV / LFO / MOD slot cycling with the page buttons.** Pressing a page button
repeatedly cycles through its slots, so double-tapping ENV/LFO gets you to LFO 2
directly. Good for muscle memory and two-handed operation.

**Turbo patch name editing.** Jump straight to `A`, `a` or `1`, insert a space,
or move the cursor with the buttons — `A | a | 1 | _ | <- | -> save | exit`.
Renaming a patch stops being a chore.

**Fast preset browsing.** Scrolling through presets no longer loads each one as
you pass it. The name updates immediately and the patch loads once you settle,
after a delay you set yourself (`ldly` on preferences page B, in milliseconds;
0 restores the old behaviour). Previously every detent triggered a full patch
load — a complete file parse, a voice-card parameter push, and sometimes a write
back to the card.

**Sequencer display.** Rests show as `---`, tracker style, which makes drum
patterns far easier to read at a glance.

**Performance page.** The first four buttons act as part mutes — good for
jamming with the chord sequencer. Button 5 resyncs the part clocks.

### Sound

**`drm1` / `drm2` modulation destinations.** Like the Osc1/Osc2 pitch
destinations but with **±32 semitones** of range, for pitch envelopes steep
enough to make actual drums. The standard coarse-pitch destinations do not reach
far enough, and the stock envelope curves are not steep enough either — but an
envelope can be *squared* with a modifier to sharpen it:

```
Modul.  1 | srce mod1 | dest drm1 | amnt  32
Modif.  1 | in1  env1 | in2  env1 | oper prod     <- squares Env1
```

That combination gets close to 808-style kicks and toms.

**Solo polyphony mode.** Like mono, but uses a single voice. Useful when a patch
was designed for one voice (legato, glide) and you want it on a part with several
voices assigned, without the stacked/unison sound.

**Arp latch mode.** Behaves like the Microkorg's: press a key once for note on,
again for note off.

**Chord sequencer mode.** The sequencer passes each block of four consecutive
notes to the arpeggiator as a chord. `lenp` sets how many steps between chord
changes; the note sequence is locked to 16 steps (four chords). Rests reduce the
number of notes in a chord.

### MIDI

**Selectable CC maps** (preferences page B): Ambika standard, Shruthi XT, or
Novation Launchkey.

**SysEx patch name query.** A host can ask the Ambika for the name of any stored
patch, sequence, program or multi without loading it — `0x16`/`0x17`/`0x18`/`0x1a`
with the bank as the argument byte and the slot in `data[0]`. Intended for editor
software; this is the groundwork for preset-name sync in a future AU/VST editor.

### Source

`common/features.h` carries compile-time switches for most of the above, so
features can be disabled to recover flash while experimenting. Each switch is
annotated with whether it actually does anything — several inherited ones did
not.

---

## Stability

Earlier builds of this fork, and stock YAM, can exhaust the controller's 4 KB of
SRAM: the stack grows down into static data and corrupts it. It shows up as rare,
unreproducible misbehaviour, usually after loading patches.

v1.5 fixes it. The main cause was **link-time optimisation**, which Emilie's
original build never used and which was added later while fighting the flash
limit on a modern compiler. LTO inlines across translation units, so local
variables that used to occupy separate, reused stack frames end up alive at the
same time. The largest single stack frame on the controller was **128 bytes**
with LTO and **58** without — in FatFs `check_fs`, which every file operation
reaches.

Measured peak stack use dropped from **336 bytes to 254**, against 312 available.

Also fixed in v1.5: the 4.9 kHz timer interrupt could re-enter itself under a
MIDI flood, each nesting costing another stack frame; 41 bytes of static SRAM
were being reserved for a buffer nothing read; and the undo snapshot ran a full
file save on *every* patch load with unsaved edits.

`make -f controller/makefile ramsize` reports static SRAM against the limit.
Note that passing it does not prove a build fits — the rule reserves 128 bytes
for the stack and the real peak is about twice that.

---

## Installing

Download `AMBIKA.BIN` from the [latest release](../../releases/latest), copy it
to the root of the SD card, then either:

- **Library → more → Firmware update**, or
- power off, hold **S8** (rightmost button) while powering on, and release when
  `SD update...` appears.

The OS information page will show the version. Keep a backup of your current
firmware first.

Voice cards are unchanged from v1.1 and do not need reflashing. The v1.1 image
is attached to releases for completeness.

---

## Building

You need Python 3, GNU Make, AVR GCC/avr-libc and AVR binutils, with the AVR
executables on `PATH`. Releases are built with GCC 9.5.0 and binutils
2.46.0.20260210.

```sh
python3 tests/run_controller_ui_tests.py --sanitizers undefined
python3 scripts/build_firmware.py --target controller --variant release /tmp/kz-build
```

`--target voicecard` builds the voice card instead, and `--variant diagnostic`
builds the memory/CPU diagnostic image. The output directory must not already
exist; the script builds in a temporary tree, enforces the flash and SRAM limits,
and packages BIN/HEX/ELF with a map, build log, feature switches and a SHA-256
manifest of every source input.

The modified `avrlib` is committed directly — no submodule setup, and no
resource regeneration needed for an ordinary build.

---

## Credits

Original Ambika by **Emilie Gillet** (Mutable Instruments). YAM fork by
**bjoeri**. Some modernisation work derives from **MachFour**'s fork. Firmware
is GPL-3.0; hardware documentation is CC-BY-SA 3.0.
