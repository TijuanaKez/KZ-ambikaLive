# v1.5 diagnostic build (2026-09-17)

Same source and commit as `../release-v1.5-2026-09-17/`, built with
`-DDIAGNOSTIC_BUILD`. Flash 55,278. Static SRAM 3,798, leaving **298 bytes** of
stack against a measured peak of 254.

**Click the encoder** to switch between the memory view and the ordinary
firmware-update page, so voice cards can still be flashed from here.

```
RAM  nnnnn LOW  nnnnn MID nnn RST xx  ctx
AUD nnn nnn nnn nnn nnn nnn  clk:fw     exit
```

- **LOW** — untouched-stack watermark. 44 on the hardware this was verified on.
- **ctx** — which path was running when the stack reached its deepest point:
  `lod`, `sav`, `bak`, `sys`, `sdt`, `mid`, `ui `, `idl`.
- **MID** — MIDI output queue high-water mark, against its 128-byte size.
  Reached 28 under a deliberate controller flood.
- **AUD** — audio CPU load per voice card, in ticks per 40-sample block, so 40
  is 100% of budget. Reads `--` for a card whose firmware predates the counter,
  which includes every v1.1 card. `254` means that card's buffer ran dry.

This is the instrument the v1.5 stack work was done with, and it is meant to stay
usable: every new oscillator in the v2 plan should have its cost recorded here.
