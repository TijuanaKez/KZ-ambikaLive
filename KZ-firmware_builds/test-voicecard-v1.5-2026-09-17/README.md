# Voice card test build v1.5 (2026-09-17)

Reports **v1.5**. Supersedes the v1.4 test build.

## Fixed: `fmfb` pitch

`Voice::ProcessBlock` skips adding the range/coarse-tune control to the pitch for
FM shapes, because those shapes use `range` as the *modulator frequency ratio*
instead (`set_fm_parameter` is `range + 36`). This tree tested only
`WAVEFORM_FM`; YAM tests `WAVEFORM_FM` **and** `WAVEFORM_FM_FB`. So on `fmfb` the
coarse tune was being applied twice over — once as a ratio and again as a pitch
offset — detuning the carrier.

## Still unexplained: `pad`, `qpwm` and `fm`

Carey reports all four roughly a semitone flat against a v1.1 card. Two things
are now established about that:

- **It is not caused by the dispatch fix.** `pad` (15) and `fm` (16) resolved to
  `RenderQuadSawPad` and `RenderFm` in the old broken table exactly as they do in
  the corrected one, so their behaviour is unchanged by that work. The difference
  predates it and belongs to the MachFour refactor.
- `qpwm` and `fmfb` were *silent* before the dispatch fix, so there is no earlier
  behaviour to compare them against. They share renderers with `pad` and `fm`
  respectively, so they inherit whatever is wrong there.

`RenderQuadSawPad` reads as mathematically identical to YAM's `RenderQuad` —
same spread calculation, same `>> 10` summation, same phase update, and
`highWord24()` is the same high 16 bits as YAM's `.integral`. So `pad` should not
differ, and the cause is not yet found.

`RenderFm` does differ from YAM's in three ways, any of which changes the sound:
it indexes `lut_res_fm_frequency_ratios` with `fm_parameter & 63` where YAM uses
a clamped `fm_parameter - 24`; it computes the modulator increment from the full
24-bit phase increment where YAM uses the 16-bit integral part; and it has no
feedback path at all, so `fmfb` currently renders as plain FM.

## The test that would narrow it

Build a patch using `pad` with **range 0, detune 0, and the oscillator parameter
(spread) at 0**, and compare it against a v1.1 card.

- **Same pitch at parameter 0, drifting apart as the parameter rises** — then it
  is the detune spread, and the renderer inputs differ rather than the renderer.
- **Different even at 0** — then it is neither the spread nor the pitch table,
  and the next place to look is the phase accumulator itself.

Flash and SRAM unchanged: 19,012 / 32,256 and 1,071 / 1,920.
