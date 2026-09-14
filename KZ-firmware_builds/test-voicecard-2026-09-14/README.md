# Voice card test build (2026-09-14) — UNTESTED, DO NOT SHIP

First voice card image compiled from this tree on the modern toolchain
(AVR GCC 9.5.0). It builds cleanly and fits, and **that is all that is known
about it.** It has never been run on hardware.

Flash 30,220 / 31,744. Static SRAM 1,072 / 2,048, leaving 976 bytes.

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
   number — see the manual).
2. Install it on card 1 through the controller's firmware-update page.
3. Play card 1 alone. Listen for: silence, wrong pitch, aliasing that the other
   cards do not have, stuck notes, and whether the filter and VCA still track.
4. Compare directly against an untouched card on the same patch.

If it is silent, that confirms the compiler hazard is real on GCC 9 and it
becomes the first thing to investigate before any v2 voice card work — since
every part of the v2 plan depends on compiling this firmware.

If it works, it is a significant result: it means the v2 work can start from a
toolchain that is known good on both halves of the synth.
