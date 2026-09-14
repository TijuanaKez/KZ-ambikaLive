# v1.4 diagnostic build (2026-09-14)

Same source as `../release-v1.4-2026-09-14/`, built with `-DDIAGNOSTIC_BUILD`.
Use this one to read the stack watermark after the deferred-load change.

Flash 51,866 / 61,440. Static SRAM 3,829 / 3,968. Runtime headroom 267.

It boots straight to the memory screen showing `RAM`, `LOW` and `RST`, and it
replaces the application firmware-update page — so it can only be reflashed by
holding **S8** (rightmost button) at power-on with `AMBIKA.BIN` on the card root.
Rename `AMBIKA_DIAG3.BIN` accordingly.

**What to look for:** `LOW` was 23 bytes on the v1.3 baseline. The deferral timer
adds 3 bytes of static SRAM, so expect roughly **20**. Browse hard, change
settings, play, then check it again. If `LOW` reaches 0 the stack has collided
with static data and the change must be reverted or paid for elsewhere before
v1.4 ships.

The bootloader can clear MCUSR, so `RST 00` is inconclusive.
