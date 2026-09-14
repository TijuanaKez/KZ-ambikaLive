# Stability milestone — controller v1.3, September 14, 2026

Carey signed off DIAG3 as stable on hardware and authorized publishing this as a
public release. **v1.3 is a stability milestone, not a feature release.** The
version number continues the v1.2 that KZ units currently report on the OS
information page; `kSystemVersion` is now `0x13`.

Tags: **`v1.3`** (published release) and **`stable-controller-2026-09-14`**
(the same commit, named for the hardware sign-off).

## What was signed off, and what was not

The image Carey ran and accepted is
[`AMBIKA_DIAG3.BIN`](../KZ-firmware_builds/diagnostic-2026-09-14-diag3/AMBIKA_DIAG3.BIN).

The shipped image is
[`AMBIKA.BIN`](../KZ-firmware_builds/release-v1.3-2026-09-14/AMBIKA.BIN). It is
built from the same source and the same commit, differing only by the absence of
`-DDIAGNOSTIC_BUILD`. That define swaps the OS information page for the
memory-only diagnostic screen and paints SRAM at startup; everything else,
including all the stability fixes, is common to both.

**The release image has not itself been run on hardware.** The hardware evidence
is DIAG3's. Treat the first flash of v1.3 as a confirmation step, not a formality.

- Controller: ATmega644P, 20 MHz.
- Compiler: Homebrew AVR GCC 9.5.0; BIN conversion with GNU Binutils
  2.46.0.20260210. Complete flags and feature settings are in each package manifest.
- Controller UI/navigation tests pass under UBSan with checked host substitutes.
  ASan could not start on this Mac; no ASan pass is claimed.
- User testing covered multi browsing, settings changes and general synth use.
  DIAG2 reached LOW=23 with no observed zero; DIAG3 received the stability
  sign-off after the bottom-right-knob preferences navigation was fixed. No
  separate numerical LOW result was supplied for DIAG3.
- The working voice-card firmware stayed installed throughout. This is a
  controller sign-off only; no freshly compiled voice-card image is validated,
  and the voice card remains at v1.1.

## Memory

| Image | Flash | Limit | Static SRAM | Limit |
|---|---:|---:|---:|---:|
| v1.3 release `AMBIKA.BIN` | **51,924** | 61,440 | **3,826** | 3,968 |
| DIAG3 `AMBIKA_DIAG3.BIN` | 51,252 | 61,440 | 3,826 | 3,968 |

270 bytes of SRAM remain for stack and any runtime use.

### Flash headroom against earlier builds

- The controller BIN published on GitHub for v1.2 is **60,616 bytes** — only 824
  bytes under the limit. Against that, v1.3 frees **8,692 bytes**, which is the
  headroom the planned improvements will draw on.
- The preserved local August 2020 BIN is 51,724 bytes, so v1.3 is 200 bytes
  *larger* than that one.

These are three different feature configurations, not a controlled measurement
of compiler savings. Most of the difference from the published v1.2 comes from
the `features.h` switches, not from GCC 9. Do not cite any of these numbers as a
toolchain benchmark.

## Retained limitations

This baseline is a starting point for improvements, not closure of every audit
lead. Still open: the small measured stack margin, dormant feature-switch page
table mismatches, the NRPN lookup lead, and ordinary OS-page port bounds — all
documented in the handover. Measure SRAM, stack and timing before adding features.

DIAG3 replaces the application firmware-update action, so it can only be
reflashed by holding **S8 (rightmost button)** at power-on with `AMBIKA.BIN` in
the SD root. The release image keeps the normal in-application update page. The
bootloader can clear MCUSR, so RST=00 is inconclusive.

## Reproduction

From a normal clone, install the recorded AVR toolchain and place its binaries on
PATH. No submodule initialization is needed; the tested avrlib is committed.

```sh
python3 tests/run_controller_ui_tests.py --sanitizers undefined
python3 scripts/build_firmware.py --variant release /tmp/kz-controller-rebuild
python3 scripts/build_firmware.py --variant diagnostic /tmp/kz-diag3-rebuild
```

Each output directory must not already exist. Compare the resulting BIN hashes
against the `manifest.json` in the corresponding package directory; DIAG3
reproduces byte-identically. `build/` is ignored and is not the source of any
released image.
