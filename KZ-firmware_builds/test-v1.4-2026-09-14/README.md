# v1.4 test build — deferred library load (2026-09-14)

**Not released. Not yet run on hardware.** This is the build to test on the unit.
The OS information page reports **v1.4** so it cannot be confused with the
published v1.3.

| Budget | This build | v1.3 | Limit |
|---|---:|---:|---:|
| Flash | 52,550 | 51,924 | 61,440 |
| Static SRAM | **3,829** | 3,826 | 3,968 |
| Runtime headroom | 267 | 270 | — |

The 3 extra bytes of static SRAM are the deferral timer. Peak stack use should
be unchanged, so the expected `LOW` reading is around **20** rather than 23.
That is the main thing to confirm — see the diagnostic build in
`../test-v1.4-2026-09-14-diag/`, which is the same source with the memory screen.

## What to test

**1. The new preference.** Preferences page B (`prefs` -> `more->`) now has a
third setting, `ldly`, displayed in milliseconds. Range 0–2000 in 10 ms steps.

- `0` disables deferral and restores the old load-on-every-detent behaviour.
- Start at **500**. Carey's guess; the whole point of the setting is that it can
  be dialled in on the unit without reflashing.

The value is stored in one of the reserved settings bytes, so the EEPROM record
is still 16 bytes and existing preferences are not disturbed. It defaults to 0
on a unit whose settings have not been re-saved, meaning **old behaviour until
you set it**.

**2. Browsing.** With `ldly` above 0, scrolling the slot encoder should update
the displayed name immediately but not load. The patch loads once the encoder
has been still for the configured time. Check that:

- Fast scrolling through a bank is noticeably quicker than v1.3.
- The patch that ends up loaded is the one shown when you stopped.
- Pressing any button while browsing loads the shown patch first.
- Leaving the library page loads the shown patch rather than stranding it.
- Program-change messages now go out once per settled patch, not once per
  detent. Worth checking if anything downstream is listening.

**3. The SysEx name query.** See below; this needs a host tool, not the unit.

## What changed

- **Deferred load.** `Library::OnIncrement` used to call the full
  `storage.Load()` on every detent: a complete RIFF parse, a voice-card
  parameter push, and a snapshot write to the SD card when the patch had unsaved
  edits. It now reads only the name chunk and arms a timer; `TickDeferredLoad`
  performs the real load once the encoder settles. The `KEZ TODO` on that line
  is now done.
- **Fixed a buffer overflow in the name reader.** The RIFF `name` chunk length
  was read from the file and used unchecked as the length of a read into a
  16-byte buffer. Any file declaring a longer name chunk — corrupt, truncated,
  or written by another tool — overwrote whatever followed. With ~20 bytes of
  stack headroom that is not a theoretical concern. The read is now clamped and
  the remainder skipped.
- **Fixed the SysEx name query.** `SYSEX_REQUEST_*_NAME` (0x16–0x18, 0x1a) was
  already scaffolded but never worked: `Load()` fills a caller-supplied buffer
  and never sets `location.name`, so the handler passed a null pointer to
  `SysExSendRaw`, which nibblized **16 bytes read from address 0** — the AVR
  register file — out over MIDI. It now supplies a real buffer, rejects
  out-of-range banks, and always replies (a blank name for an empty or invalid
  slot) so an editor can tell "empty" from "no answer".

## SysEx name query, for the plugin side

Request, after the standard Ambika SysEx header:

| Byte | Meaning |
|---|---|
| command | `0x16` patch, `0x17` sequence, `0x18` program, `0x1a` multi |
| argument | bank, 0–25 (A–Z) |
| data[0] | slot, 0–127 |

The reply uses the same command and argument bytes, followed by the 16-byte
name, nibblized high-then-low per byte like every other Ambika SysEx payload.
Names are space-padded with a terminating null.

One request per name. A bank of 128 is 128 round trips, which is fine for
populating an editor in the background but is not an instant operation. If that
proves too slow, the next step is a bulk range request — deliberately not built
yet, because a reply buffer would cost static SRAM the controller does not
currently have.
