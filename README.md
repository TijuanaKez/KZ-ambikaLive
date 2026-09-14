# KZ Ambika Live

The single source repository for Carey's KZ Ambika firmware, incorporating the
preserved local KZ work and controller stabilization on AVR GCC 9.5.0.

**Current release: v1.4 (2026-09-14)** — faster preset browsing.
Download [AMBIKA.BIN](KZ-firmware_builds/release-v1.4-2026-09-14/AMBIKA.BIN):
flash **52,550 bytes**, static SRAM **3,829 bytes**. Voice-card firmware is
unchanged at v1.1 and does not need reflashing.

v1.3 before it was the stability milestone: the first published build on the
modern AVR GCC 9 toolchain, with the preferences-page memory corruption fixed.
Earlier KZ units report v1.2.

- [v1.4 release notes and installation](KZ-firmware_builds/release-v1.4-2026-09-14/README.md)
- [v1.3 release notes](KZ-firmware_builds/release-v1.3-2026-09-14/README.md)
- [Baseline, hardware sign-off and limits](docs/STABLE_BASELINE.md)
- [Current handover and improvement plan](docs/KZ_AMBIKA_CODEX_HANDOVER.md)
- [Repository history and consolidation](docs/REPOSITORY_CONSOLIDATION.md)
- [Voice card v2 architecture plan](docs/VOICECARD_V2_PLAN.md) (breaking; not started)
- [DIAG3 memory-diagnostic image](KZ-firmware_builds/diagnostic-2026-09-14-diag3/README.md)

## Install

Copy `AMBIKA.BIN` to the SD card root, then either use **Library -> more ->
Firmware update**, or power off and hold **S8** (rightmost button) while powering
on. Keep a backup of your working firmware first.

## Build

Use Python 3, GNU Make, AVR GCC/avr-libc and AVR binutils. Put the AVR executables
on PATH. The recorded build uses GCC 9.5.0 and binutils 2.46.0.20260210; other
versions require their own size and behavior checks. The modified `avrlib/` is
included directly; no submodule checkout or resource regeneration is needed.

```sh
python3 tests/run_controller_ui_tests.py --sanitizers undefined
python3 scripts/build_firmware.py --target controller --variant release /tmp/kz-build
```

Pass `--variant diagnostic` for the DIAG3 memory-instrumented image, or
`--target voicecard` for the voice card. **No freshly compiled voice card image
has ever been validated on hardware here** — see
`KZ-firmware_builds/test-voicecard-2026-09-14/README.md` before flashing one. That
image boots straight to a RAM/stack screen and replaces the firmware-update page,
so it can only be reflashed with the hold-S8 method.

The output directory must be new. The script builds in a fresh temporary directory,
checks flash <61,440 and static SRAM <3,968, and packages BIN/HEX/ELF, a map, build
log and exact source manifest. The test command needs a host C++ compiler
(`clang++` by default, or `CXX`). It tests UI logic, not AVR interrupt timing.

`common/features.h` controls optional features and modifications; record its
configuration for every build. Do not treat old files in the ignored `build/`
directory as freshly built firmware.

## About Ambika
A hybrid MIDI polysynth and voicecard host.

Ambika consists of a compact motherboard serving as a "host" for up to 6 sound synthesis voicecard. While this design is primarily intended to be a flexible hybrid polysynth, it could also be used as a drum module/drum machine.

The motherboard comprises 6 audio outputs, each one connected to a voicecard ; a global mono output ; a pair of MIDI input/output ; a SD card slot ; a 5V/8V/-8V power supply capable of delivering 150mA on the 8V rails and 350mA on the 5V rail ; the master MCU and the user interface elements. The voicecards (a pair of each being attached to the 3 voicecard ports) are SPI slaves, they receive note and modulation data from the motherboard ; and output monophonic audio, ideally 1V pp.

3 designs of voicecards implementing a refined version of the Shruthi-1 engine are provided. Each of those use a different filter (4-Pole with LM13700, 4-Pole with SSM2164, 2-Pole SVF with SSM2164).

Original developer: Emilie Gillet (emilie.o.gillet@gmail.com)

The firmware is released under a GPL3.0 license. It includes a variant of the formant synthesis algorithm used in Peter Knight's Cantarino speech synthesizer.

The PCB layouts and schematics, documentation, analyses, simulations and 3D models are released under a Creative Commons cc-by-sa 3.0 license.

