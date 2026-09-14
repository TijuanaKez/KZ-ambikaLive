# KZ controller DIAG3 — preferences navigation knob fix

DIAG2 was reported stable during browsing multis, settings changes and general
synth use, with LOW reaching 23 bytes and no observed zero. That is a narrow
observed untouched-stack margin; it is not a worst-case stack guarantee.
This DIAG3 image has not yet been tested on hardware.

## Changes from DIAG2

DIAG2 ignored pot events over `more->`/`<-back`. DIAG3 handles those navigation
events directly, without indexing the parameter table or writing settings bytes.
Treating Carey's "last encoder" as the bottom-right parameter knob:

- Turn that knob clockwise beyond the centre region to open preferences B.
- Turn it anticlockwise beyond the centre region to return to preferences A.
- A dead band (values 48–79 out of 0–127) keeps continued movement or small
  variations from immediately switching back. Crossing 80 selects B; crossing
  47 on the way down selects A. Navigation bypasses parameter snap mode.
- Main-encoder click/scroll navigation still works.

Other firmware changes from DIAG2 are only the on-screen build labels. The
stack diagnostic implementation and feature switches remain the same.

## Build and test

- File: `AMBIKA_DIAG3.BIN`, controller only. For SD updating, copy as `AMBIKA.BIN`.
- Flash: **51,252 bytes**, below 61,440 (54 bytes more than DIAG2).
- Static SRAM: **3,826 bytes**, below 3,968 and unchanged from DIAG2.
- Runtime headroom before stack use: 270 bytes. Hardware LOW may differ.
- GCC 9.5.0; exact source, flags, hashes and artifacts are in this package.
- Fresh build passed with the existing unused `power_status` warning only.
- UBSan host tests passed, including full knob sweeps in both directions,
  both snap settings, entry from pot editing, single transitions, no parameter
  accesses/writes during navigation, invalid control indexes and existing
  encoder/rendering tests. ASan remains unavailable due to the previously
  recorded macOS startup failure.

Keep the DIAG2/working controller backups and current voice-card firmware. After
installing DIAG3, record startup RAM/LOW, then exercise the bottom-right knob on
both preferences pages. Return to diagnostics via Library → more → DIAG3 and
report LOW after browsing/settings use. RST may be 00 because the bootloader
clears reset flags; LOW is a stack-paint estimate, not a proof of stack safety.

Rebuild to a new directory with:

```sh
python3 scripts/build_controller_diagnostic.py KZ-firmware_builds/diagnostic-new
```

No hardware was flashed by Codex.
