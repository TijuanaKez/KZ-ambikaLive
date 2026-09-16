# KZ Ambika Live — controller v1.5 (2026-09-17)

**The stack release.** v1.3 and v1.4 could both exhaust the controller's stack
and corrupt memory — and so, it turns out, can the stock YAM firmware everyone
runs. This finds the cause and fixes it.

| | v1.4 | **v1.5** | Limit |
|---|---:|---:|---:|
| Flash | 52,550 | 54,262 | 61,440 |
| Static SRAM | 3,829 | **3,784** | 3,968 |
| Stack headroom | 267 | **312** | |
| **Measured peak stack** | 336 | **254** | |

Controller only. Voice cards are unchanged.

## What was wrong

**Link-time optimisation.** `pichenettes/avril` never used `-flto`; MachFour added
it while fighting the flash limit on a modern compiler, and this tree inherited
it. LTO inlines across translation units, so locals that used to occupy
separate, reused stack frames end up live simultaneously. The largest single
frame on the controller went from 58 bytes to **128** — in FatFs `check_fs`,
which every file operation reaches through `chk_mounted`.

Turning it off cost 2 KB of flash and cut the measured peak from 336 bytes to
254. `LTO_FLAGS` is now a makefile variable; the voice card keeps LTO, being
short of cycles rather than stack.

## Also fixed

- **TIMER1 could interrupt itself.** It carried `ISR_NOBLOCK`, which re-enables
  interrupts on entry, so under a MIDI flood a pass could exceed its 205 µs
  period and re-enter — each nesting costing another frame of the deepest
  interrupt in the firmware. Now bounded to one, with an atomic test-and-set.
- **41 bytes of dead SRAM** reclaimed: `ui.cc` had a `static char line[41]` that
  `Ui::Init()` filled and nothing ever read.
- **The undo snapshot is compiled out** (`DISABLE_SNAPSHOT` in `features.h`). It
  ran a full `Storage::Save` plus an `Unlink` on *every* patch load with a dirty
  edit buffer — the deepest path in the firmware, on the most ordinary
  operation there is. Paste, Swap and Init still work; they no longer record
  undo history. Undefine the switch to get it back.
- Ring buffer sizes now `static_assert` on being a power of two. The arithmetic
  masks with `size - 1`, so 96 would have corrupted silently.
- `make -f controller/makefile ramsize` restored — it was lost in the MachFour
  refactor. Its comment now records that passing it does not prove a build fits:
  the rule reserves 128 bytes for the stack and the real peak is 254.

## Verification

Confirmed on hardware by Carey: `LOW` bottomed at 44 with 298 bytes of headroom
on the matching diagnostic build, including a deliberate program save with
`autobackup` on — which calls `f_unlink`, `f_rename` and `f_open`, the three
deepest frames in the firmware.

Host UI tests pass under UndefinedBehaviorSanitizer. A fresh clone rebuilds this
image byte for byte.

The release image differs from the tested diagnostic one only by the absence of
`-DDIAGNOSTIC_BUILD`, and has more stack headroom than it (312 against 298).

## Install

Copy `AMBIKA.BIN` to the SD card root, then **Library → more → Firmware update**,
or hold **S8** at power-on. The OS information page will report **v1.5**.
