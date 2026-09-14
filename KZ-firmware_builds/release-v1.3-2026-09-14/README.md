# KZ Ambika Live — controller v1.3 (2026-09-14)

Shipping controller firmware. This is the stability milestone: the same source
that produced the hardware-signed-off DIAG3 image, built without
`DIAGNOSTIC_BUILD` so the normal firmware-update page is restored.

| Budget | This build | Limit |
|---|---:|---:|
| Flash | **51,924** | 61,440 |
| Static SRAM (`.data + .bss + .noinit`) | **3,826** | 3,968 |
| Runtime headroom (stack) | 270 | — |

The OS information page now reports **v1.3** (`kSystemVersion = 0x13`). Voice-card
firmware is unchanged at v1.1 and does not need reflashing.

## What is in it

- Builds clean on the modern toolchain: **AVR GCC 9.5.0**, binutils 2.46.0.20260210.
- Correctness fixes: AVR clock and SysEx hazards, voicecard SPI receive bounds,
  fixed-width parameter string rendering bounds.
- Preferences `more->` / `<-back` no longer index the parameter table. This was
  the cause of the display corruption and instability when entering preferences
  page B. Both the main click encoder and the bottom-right parameter knob now
  navigate (knob: clockwise past 80 for `more->`, anticlockwise below 47 for
  `<-back`; the thresholds are a dead band so a continued turn only switches once).
- Leaving preferences page B now saves its settings; previously it did not.
- Compile-time checks that the page registry matches `UiPageNumber`, the unit and
  parameter tables match their enum sentinels, and the 16-byte EEPROM settings
  record keeps its byte offsets. Feature switches that would break these are now
  build errors rather than runtime corruption.

## Install

Controller only. Keep a backup of your current working firmware first.

1. Copy `AMBIKA.BIN` to the root of the SD card.
2. Power the Ambika off.
3. Hold **S8** (rightmost button) while powering on.
4. Release when `SD update...` appears and let it finish.

From v1.3 onward the in-application route also works again: **Library → more →
Firmware update**, since the release build does not replace that page.

## Verification status

- Host UI tests pass under UndefinedBehaviorSanitizer.
- Both memory budgets enforced by the build script, which fails rather than
  emitting an oversized image.
- Hardware: DIAG3, byte-identical in source, was signed off after browsing
  multis, changing settings and general play. Lowest observed untouched-stack
  reading (`LOW`) was **23 bytes** — a narrow margin. Do not add SRAM-consuming
  features before the runtime stack investigation is done.
- This exact release image has not itself been run on hardware yet; it differs
  from DIAG3 only by the `DIAGNOSTIC_BUILD` define.

## Reproduce

```sh
python3 tests/run_controller_ui_tests.py --sanitizers undefined
python3 scripts/build_controller.py --variant release <new-output-dir>
```

`features.h` in this directory records the exact feature switches used.
`manifest.json` records compiler versions, the make command and SHA-256 for
every source input and artifact.
