# KZ controller DIAG2 — September 14, 2026

Hardware feedback, September 14, 2026: Carey reports apparently stable operation
after browsing multis, changing settings and general synth use. LOW reached
**23 bytes**, but has not been seen at zero. This is a narrow observed margin;
the baseline is not yet declared fully validated. Preferences `more->` does not
respond to the expected last control and needs a follow-up control-path fix.

## Image and memory

- Controller only: `AMBIKA_DIAG2.BIN`.
- Flash: **51,198 bytes**, below 61,440.
- Static SRAM: **3,826 bytes** (.data 58 + .bss 3,768), below 3,968.
- Remaining SRAM for runtime: **270 bytes**; the static limit alone does not prove stack safety.
- Compiler: Homebrew AVR GCC 9.5.0; ATmega644P, 20 MHz, `-Os`, LTO and `-mcall-prologues`.
- Existing `common/features.h` switches retained; additional compiler define `DIAGNOSTIC_BUILD`.
- SHA-256: `17f7c4531c0bebdcdbe8373779754fd6fe7b3719dc91dba169ff3e4023389f88`.

## Hardware test

1. Retain the known-working controller backup. Install this controller image using
   the existing update procedure. For the SD loader, copy it to the card root as
   `AMBIKA.BIN`. Leave the working voice-card firmware installed.
2. After initialization, the screen should open automatically to **KZ DIAG2**.
   Record **RAM**, **LOW** and **RST** before doing anything else.
3. Press button 8 (`exit`). Test preferences A → `more->` → preferences B →
   `<-back`, both by scrolling and by clicking the navigation control. Move the
   pots, including those over blank/navigation cells.
4. Return using Library → `more` (button 7) → `DIAG2` (button 3). Record the values.
5. Browse saved multis and repeat the operations that previously caused
   corruption. Revisit DIAG2 between operations. Report the first operation that
   fails and the latest readings. If LOW reaches zero, report it before further
   stress testing.

The diagnostic OS page only displays memory/reset information and exits; it
does not perform SD scans, query voice-card versions or trigger firmware updates.
Subsequent firmware replacement uses the existing bootloader update procedure.

### Reading the display

- **RAM**: current distance between static data and the sampled stack pointer.
  It is measured on this screen, so it does not capture earlier deep calls.
- **LOW**: contiguous bytes still carrying the startup stack-paint pattern above
  static data. This estimates the untouched SRAM margin since early controller
  initialization, including subsequent interrupt/SD activity. No extra SRAM
  array is allocated. Zero indicates that the pattern at the bottom was touched;
  it does not by itself identify which code wrote it.
- **RST**: MCUSR captured at application startup. The checked controller
  bootloader clears MCUSR before launching the application, so `00` may simply
  mean the bootloader erased the reset flags. It cannot rule out a reset.

Stack painting is diagnostic evidence, not a proof of maximum stack use:
unchanged/reserved stack slots or writes matching the pattern can conceal use.
Painting begins below the live initialization frame, with a 16-byte guard.
The image must not use dynamic allocation; the build script checks allocator
symbols and the inspected controller has no heap allocation calls.

## Fixes and findings

- The recovered September 13 08:47 diagnostic image predates the 11:02
  `7b9467f` synthetic-control guard. It was not a test of that guard.
- Preserve raw `0xf8`/`0xf9` IDs for rendering more/back labels, while excluding
  them from parameter access. The previous guard hid the labels.
- Clicking more/back now navigates directly; pot/edit events cannot treat these
  controls or invalid assigned parameter IDs as table indexes.
- Save settings when leaving preferences B as well as A.
- Initialize all nine remembered UI groups rather than copying only eight.
- Reject out-of-range page requests. Compile-time checks verify page registry
  indexes, the UNIT_LAST table, the parameter count, and the 16-byte settings
  layout/new parameter offsets. `padding[6]` remains unchanged.
- A real dormant enum/table mismatch exists with `DISABLE_SEQUENCER` or
  `DISABLE_VERSION_MANAGER`: each removes a table entry without removing the
  enum value. These configurations now fail the page-registry assertion. The
  current feature configuration does not enable either switch.

The hardware cause is not yet proven. This image removes confirmed unsafe paths
and supplies earlier/better diagnostics; it does not establish that all memory
corruption has been eliminated.

## Validation and reproduction

- Fresh diagnostic and ordinary controller builds succeeded. Ordinary build:
  flash 51,870; static SRAM 3,826. The ordinary build retains the update page.
- Both builds have one existing warning: unused `power_status` in FatFs `mmc.c`.
- `python3 tests/run_controller_ui_tests.py --sanitizers undefined` passed.
  Tests compile the actual parameter-editor and diagnostic-page methods against
  checked host substitutes, covering more/back display/click/scroll, unused pots,
  invalid IDs, ordinary editing and display boundaries. They do not model AVR
  interrupts, PROGMEM or SD hardware.
- AddressSanitizer could not run on this Mac: startup SIGILL in
  `__pthread_init`, reproduced outside the sandbox. No ASan pass is claimed.
- Negative syntax checks confirmed that the two mismatched feature-switch
  configurations above fail with the expected page-registry assertion.
- Inspected AVR disassembly: watermark painting occurs after CLI, before SEI,
  uses register-only byte writes below SP minus 16, and makes no library calls.
  SP sampling preserves interrupt state. No allocator symbols are linked.

Build from the repository with AVR tools on PATH:

```sh
python3 scripts/build_controller_diagnostic.py KZ-firmware_builds/diagnostic-new
```

The destination must be new. `manifest.json` records exact tool versions,
arguments, source hashes and artifact hashes. `source.tar.gz` contains the actual
source inputs, including local avrlib, generated resources and pending diagnostic
files. The source snapshot can be extracted into an empty directory for rebuilding;
the packager also expects Git metadata for provenance. ELF, HEX, map, section sizes
and the full build log are included here. No hardware was flashed by Codex.
