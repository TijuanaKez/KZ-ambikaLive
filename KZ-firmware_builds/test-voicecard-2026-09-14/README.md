# Voice card test build (2026-09-14) — UNTESTED, DO NOT SHIP

First voice card image compiled from this tree on the modern toolchain
(AVR GCC 9.5.0). It builds cleanly and fits, and **that is all that is known
about it.** It has never been run on hardware.

Flash 30,220 / 31,744. Static SRAM 1,072 / 2,048, leaving 976 bytes.

**It reports v1.2 on the OS information page**, against v1.1 for every untouched
card. The voice card version is display-only -- the controller reads it but never
gates on it -- so bumping it is safe, and without it a flashed card would be
indistinguishable from an unflashed one.

## Why it is not in the v1.4 release

Carey's historical notes, recorded in the handover, say that later compilers
could build the voice card while producing a **silent** card. Nobody has checked
whether GCC 9 is one of them. A silent voice card is not a small inconvenience:
it needs an ISP programmer to recover if the SD route does not work.

The v1.4 release therefore ships the known-good **v1.1** voice card binary
(`../legacy-v1.2-published/ambika_voicecard_v1.1.bin`, 26,160 bytes), which is
what units are already running and what v1.4 was tested against.

Note the size difference: 30,220 here against 26,160 for the known-good image.
That is a different feature configuration as well as a different compiler, so
these are not two builds of the same thing. Do not read the gap as bloat.

## Testing it, when you want to

Do this with **one** voice card, not six, and keep the others on v1.1 so the
synth stays playable and the comparison is direct.

1. Copy `VOICE.BIN` to the card root as `VOICE1.BIN` (the digit is the card
   number — see the manual). Leave `VOICE2.BIN`..`VOICE6.BIN` on v1.1.
2. Open **Library → more → Firmware update**. Turn the encoder to select port
   **1** — *not* ALL. The right-hand side of the screen shows `upgrade`.
3. Press **S4**, the switch under the right-hand `upgrade`. S1 is the
   *controller* update; do not press that.
4. When it finishes, the page should show card 1 as **v1.2** and the others as
   v1.1. That is the confirmation the flash took.
5. Play card 1 alone. Listen for: silence, wrong pitch, aliasing the other cards
   do not have, stuck notes, and whether the filter and VCA still track.
6. Compare directly against an untouched card on the same patch.

To roll back, copy the v1.1 image over `VOICE1.BIN` and repeat. A known-good copy
is in this repository at `../legacy-v1.2-published/ambika_voicecard_v1.1.bin`.

If it is silent, that confirms the compiler hazard is real on GCC 9 and it
becomes the first thing to investigate before any v2 voice card work — since
every part of the v2 plan depends on compiling this firmware.

If it works, it is a significant result: it means the v2 work can start from a
toolchain that is known good on both halves of the synth.
