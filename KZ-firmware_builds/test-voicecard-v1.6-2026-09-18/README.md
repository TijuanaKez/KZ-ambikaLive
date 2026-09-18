# Voice card v1.6 (2026-09-18) — 16-bit multiply overflows fixed

Reports **v1.6**. Fixes the `pad` and `qpwm` pitch error, and a related one in
the CZ resonant shapes that nobody had noticed.

## The bug

`int` is **16 bits** on AVR, so `uint16_t * uint16_t` multiplies in 16 bits and
wraps. Two places in `oscillator.cc` put the widening cast on the *result*
instead of on an *operand*, so the value had already overflowed by the time it
was widened:

```c
KZ    U32(phase_increment_tmp * U16(parameter)) >> 13    // wraps, then widens
YAM   ((phase_increment_.integral * uint32_t(parameter_)) >> 13)   // widens first
```

**`RenderQuadSawPad`** — this sets the detune spread of the four saws. A middle
note gives roughly 700 × 255 = 178,500, well past 65,535. Measured effect:

| increment | parameter | spread was | should be |
|---:|---:|---:|---:|
| 700 | 255 | 5 | 21 |
| 1400 | 255 | 3 | 43 |
| 2800 | 255 | 7 | 87 |

Far too little spread, so the four saws sat almost on top of each other. Since
they are summed, the perceived pitch of the cluster stayed near the fundamental
instead of spreading upward — which is exactly why `pad` and `qpwm` sounded
*flat* against a v1.1 card, and why the amount differed between them: it depends
on the note and on the patch's parameter value.

**`RenderCzResoWave`** — the same mistake sets the CZ resonance frequency, so the
resonant peak was detuned rather than the fundamental. That affects all ten CZ
shapes and had not been reported, probably because a CZ timbre is hard to check
by ear against a reference.

## What this does not fix

`fm` and `fmfb` are a separate matter, and one of them is a design decision
rather than a bug — see below. Compare them again after this build; the pitch
should be unchanged from v1.5 for those two.

## FM ratio indexing — needs a decision

`RenderFm` picks the modulator ratio differently from YAM:

```c
KZ    fm_type = fm_parameter & 63          // 64 ratios, wraps
YAM   offset  = clamp(fm_parameter - 24)   // 25 ratios, clamped
```

With `fm_parameter = range + 36`, a patch with range 0 selects ratio **36** here
and ratio **12** on a v1.1 card. Different ratio, different timbre, different
perceived pitch. That is not an overflow — it is a different choice, and it gives
access to more ratios than YAM allows.

So: keep the wider range and accept that FM patches will not match a v1.1 card,
or match YAM and lose the extra ratios. Worth deciding before any FM patches get
written against it.

`RenderFm` also has no feedback path, so `fmfb` currently renders as plain FM
regardless of which indexing is used.

Flash 19,054 / 32,256. Static SRAM 1,071 / 1,920.
