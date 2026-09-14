# Voice card v1.4 — wavetables removed (2026-09-15)

Reports **v1.4**. Supersedes v1.3, which reported v1.3 and still had them.

| | v1.3 | **v1.4** | Limit |
|---|---:|---:|---:|
| Flash | 30,168 | **18,964** | 32,256 |
| Free | 2,088 | **13,292** | |
| Static SRAM | 1,074 | 1,070 | 1,920 |

**11,204 bytes freed** — more than a third of the voice card's flash, and about
7× the headroom there was before. This is what makes new oscillator algorithms
possible at all.

## What was removed

- `wav_res_waves`, the 10,320-byte PPG-style wave bank.
- `wav_res_wavetables`, its 288-byte index.
- `RenderInterpolatedWavetable` and `RenderWavequence`.

`data/waves.bin` is still in the repository, and
`voicecard/resources/waveforms.py` documents exactly what was taken out, so this
is reversible if it ever needs to be.

## Your patches still load

`WAVEFORM_WAVETABLE_1`..`_16` and `WAVEFORM_WAVEQUENCE` keep their enum slots, so
no patch byte changes meaning and nothing needs converting. Those shapes now
render a **polyBLEP saw** instead. Every other shape is untouched.

From the bank A survey: 15 of 92 patches reference a wavetable on at least one
oscillator, and will sound different. The other 77 are unaffected.

## What to check

1. The shapes you actually use sound exactly as they did on v1.3.
2. A patch that used a wavetable now plays a saw rather than silence or noise.
3. Nothing regressed in the CZ family, FM, vowel, noise or quad saw pad.
4. **AUD on the diagnostic controller** — removing code should not have changed
   the CPU cost of anything, so the reading should match v1.3. If it moved,
   something unintended changed.

## Still on the table

`WAVEFORM_OLD_SAW` is the last shape reading the bandlimited zone tables
(`wav_res_bandlimited_*`, 4,112 bytes). Retiring it as well, and reducing
`RenderSimpleWavetable` to just the sine path, would free roughly **4.3 KB**
more. It is never used anywhere in bank A. Not done here because it was not part
of the request — say the word.
