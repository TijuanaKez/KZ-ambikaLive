# Voice card v1.5 — classic build, with wavetables (2026-09-17)

For anyone who wants the 16 PPG-style wavetables and `WAVEQUENCE` kept. Built
from the same source as the v2 build with `KZ_CLASSIC_WAVETABLES` defined:

```sh
python3 scripts/build_firmware.py --target voicecard --variant classic <dir>
```

| | classic | v2 (`--variant release`) |
|---|---:|---:|
| Flash | 30,282 / 32,256 | 19,022 |
| Static SRAM | 1,073 / 1,920 | 1,071 |

The 11,260-byte difference is the wave bank (`wav_res_waves`, 10,320), its index
(288) and the two renderers.

## Compatibility

**Both builds work with the v1.5 controller, and patches load on either.** The
oscillator enum positions never move, so a patch that selects `wavetable 7`
carries the same byte for both. A classic card renders the wavetable; a v2 card
renders a polyBLEP saw instead.

You can mix cards in one instrument. A part assigned to a classic card will play
wavetables; the same patch on a v2 card will not. That is worth knowing if you
are troubleshooting why two voices of the same patch sound different.

## What it does *not* roll back

This build only restores the wavetables. Everything else in v1.5 applies:

- The oscillator dispatch fix. Before it, `saw`, `pwm` and `triangle` all
  rendered a saw, and everything from `WAVETABLE_1` upwards was silent below
  MIDI note 108. So this is not a "back to v1.1" image — it is the corrected
  firmware with the wavetables present.
- The triangle renderer ported from YAM.
- `QUAD_PWM` restored, and the `fmfb` pitch fix.
- Audio CPU load reporting over SysEx, shown as `AUD` on the diagnostic
  controller.

## Known, unresolved

`pad`, `fm` and `qpwm` sit roughly a semitone below a v1.1 card. That predates
this work — those shapes reached the same renderers before the dispatch fix —
and is still being investigated. It affects the classic and v2 builds equally.

## Install

Copy `VOICE.BIN` to the SD card root as `VOICE1.BIN` (the digit is the card
number), then **Library → more → Firmware update**, select the port, and press
**S4**. Keep a backup of your working firmware.
