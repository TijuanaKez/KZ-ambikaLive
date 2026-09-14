# Voice card test build v1.3 — oscillator dispatch fixed (2026-09-15)

Reports **v1.3** on the OS information page. The previous test build reported
v1.2; untouched cards report v1.1.

Flash 30,118 / 32,256. Static SRAM 1,072 / 2,048.

## What this fixes

The v1.2 test build made sound and tuned correctly, but Carey found that its
first three oscillator shapes — saw, PWM and triangle — **all sounded like a
saw**, rejoining the v1.1 cards at sine. That is exactly what the dispatch
analysis predicted, and it confirmed it on hardware.

`voicecard/oscillator.h` carried MachFour's `fn_table` and dispatch, written for
the *original* Ambika enum where `SAW = 1`, `SQUARE = 2` and the wavetables come
last. `common/patch.h` carries YAM's enum, where `POLYBLEP_SAW = 1`,
`POLYBLEP_PWM = 2`, `TRIANGLE = 3` and the wavetables sit in the middle at 21.
So:

- `POLYBLEP_SAW` reached `RenderSimpleWavetable`, the bandlimited saw — a saw, by
  luck.
- `POLYBLEP_PWM` was intercepted by a special case that called
  `RenderSimpleWavetable` whenever the pulse width was 0 — a saw.
- `TRIANGLE` reached `RenderSimpleWavetable`, which emits the saw zones for
  anything that is not `SINE`. The KZ tree had **no triangle renderer at all**.
- Everything from `WAVETABLE_1` up collapsed onto one slot with no offset, so
  `OLD_SAW`, `QUAD_PWM`, `FM_FB`, `POLYBLEP_CSAW` and `VOWEL_2` landed on an
  unmatched inner switch and emitted zero below MIDI note 108.

Now the table is ordered to match `common/patch.h`, the dispatch has YAM's offset
branch for shapes above the wavetable block, the pulse-width special case is
gone, and `RenderNewTriangle` is ported from the YAM voicecard — the firmware
Carey's other five cards actually run. `RenderQuadSawPad` gains the `QUAD_PWM`
branch it needs for the same reason.

Two static_asserts now pin the table length and the low enum order, so this
particular drift cannot happen silently again.

Out-of-range shape bytes are also clamped to silence. Carey's SD card really does
contain patches with shape values 59, 96 and 229, which previously indexed past
the end of the table.

## What to check

Step through the oscillator shapes on the flashed card against an untouched v1.1
card, on the same patch. **They should now agree all the way through**, in
particular:

1. **Saw, PWM and triangle are three different sounds**, matching v1.1.
2. PWM responds to the pulse-width parameter, including at 0 (a square).
3. Triangle responds to its parameter by folding.
4. The wavetable shapes render as wavetables rather than near-silence.
5. `POLYBLEP_CSAW` still works — it was correct before by luck.

Known deviations from v1.1, both inherited from MachFour and not regressions:

- Above MIDI note 107 the polyBLEP renderer reverts to a plain saw to save CPU,
  so PWM and CSAW lose their character at the very top.
- `QUAD_PWM` and `FM_FB` share renderers with `QUAD_SAW_PAD` and `FM`, keyed on
  shape, as they do on YAM.

Roll back with `../legacy-v1.2-published/ambika_voicecard_v1.1.bin` if needed.
