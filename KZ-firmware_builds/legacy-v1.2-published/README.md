# Previously published KZ firmware (v1.2 controller, v1.1 voice card)

These are the binaries that were published on GitHub before v1.3, recovered from
the published history during the September 2026 consolidation. They are what
most existing KZ Ambika units are running. Kept for rollback and comparison.

| File | Bytes | Rename on the SD card to |
|---|---:|---|
| `ambika_controller_v1.2.bin` | 60,616 | `AMBIKA.BIN` |
| `ambika_voicecard_v1.1.bin` | 26,160 | `VOICE#.BIN` (`#` = card number, see manual) |

The controller image leaves only 824 bytes of flash free against the 61,440-byte
limit. That is the headroom problem v1.3 addresses.

The voice-card image is unchanged in v1.3 — v1.3 is a controller-only release, so
there is no need to reflash voice cards.

A separate 51,724-byte local August 2020 controller build is kept at
`../ambika_controller_v1.2a.bin`; it is a different feature configuration and was
never the published binary.
