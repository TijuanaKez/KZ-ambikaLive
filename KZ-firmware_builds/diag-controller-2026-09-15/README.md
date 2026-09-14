# Diagnostic controller — with per-voice-card CPU headroom (2026-09-15)

Reports **v1.4**, same as the release, but this is the `DIAGNOSTIC_BUILD`. It
replaces the firmware-update page with the memory screen, so it can only be
reflashed by holding **S8** at power-on.

Flash 51,968 / 61,440. Static SRAM 3,836 / 3,968, leaving 260 for the stack —
7 bytes more than the release, for the headroom array. The release controller is
byte-identical to v1.4 and unaffected.

## What it shows

```
KZ DIAG3 RAM   nnnnn LOW   nnnnn        RST xx
AUD  nnn  nnn  nnn  nnn  nnn  nnn       exit
```

- **RAM** — free SRAM between the heap start and the stack pointer.
- **LOW** — untouched-stack watermark. Expected around 20 on v1.4.
- **RST** — reset cause. The bootloader can clear MCUSR, so `00` is inconclusive.
- **AUD** — new. Audio render headroom for voice cards 1 to 6.

## Reading AUD — this is the CPU budget, without a scope

Each voice card now tracks the free space left in its 128-sample audio buffer at
the moment it starts rendering a block. The ISR drains one sample per 39.2 kHz
tick and the main loop refills 40 at a time, so that number says how close the
renderer came to being late.

| Reading | Meaning |
|---|---|
| ~40–48 | Healthy. The renderer refills as soon as a block's worth is free. |
| rising above ~64 | Falling behind; the buffer is draining faster than it fills. |
| approaching 128 | Nearly starved. |
| **255** | The buffer **did** run dry. Over budget. |

The value is the peak since the last read, and reading it clears it — so it
answers "what was the worst case since I last looked", not "what is it now".
Cards are polled one per screen refresh, round-robin, because each query is a
blocking SPI transaction.

A card with no note playing is idle, so read it **while playing**, ideally a
held chord with heavy modulation. To measure one oscillator algorithm, set both
oscillators to it, play, and read.

This is the Phase 1 instrument from the v2 plan, and it is meant to stay. Every
new oscillator in Phase 4 should have its cost recorded here before the next one
is written.

## Cost

Voice card: 50 bytes of flash, 2 of SRAM, and one comparison per rendered block
(about 980 a second). It is always on, so any v1.3+ card can be queried.
Controller: diagnostic build only.
