# KZ Ambika working agreement

- This repository is the single active KZ source. Read
  `docs/KZ_AMBIKA_CODEX_HANDOVER.md` and `docs/STABLE_BASELINE.md` first.
- `origin` is `https://github.com/TijuanaKez/KZ-ambikaLive.git`; the normal branch
  is `master`. MachFour and other historical trees are references, not the
  authority for overwriting the tested KZ source.
- Carey has requested that completed work be kept synchronized with GitHub.
  Fetch before starting changes; preserve existing user work. After each coherent
  requested change, run relevant checks, commit the completed change and push to
  `origin` without requesting the same routine approval again. Never force-push,
  reset away local work, or push to an upstream author's repository.
- Keep commits focused. Do not mix speculative synthesis work into stability
  fixes. Record compiler, feature switches, flash, static SRAM and hardware status
  for each hardware-test image. Distinguish a successful build from hardware
  validation; do not replace the tested baseline binary with an untested build.
- The published release is **v1.3** (`kSystemVersion = 0x13`), tagged `v1.3`. The
  hardware sign-off behind it is DIAG3, tagged `stable-controller-2026-09-14` at
  the same commit; the release image itself has not been run on hardware. Preserve the tested BIN and its source manifest.
  Leave the working voice-card firmware installed unless a voice-card test is
  specifically intended. LOW reached 23 bytes in DIAG2; keep runtime stack
  headroom in the improvement budget even though Carey signed off stability.
- Controller flash must be <61,440 bytes; static SRAM must be <3,968 bytes.
  Voice-card flash must be <31,744 bytes. Preserve enum/table bounds, settings
  layout and `common/features.h` choices; inspect their consumers before editing.
- `avrlib/` is intentionally vendored from the exact tested local tree. Do not
  replace it with the obsolete submodule version. Original dependency revisions
  are recorded in `docs/REPOSITORY_CONSOLIDATION.md` and Git history.
- `build/` contains ignored local artifacts, including historical files; never
  assume those files match current source. `scripts/build_controller.py --variant {release,diagnostic}` builds either image
  in a fresh temporary directory and enforces both memory limits. Do not publish incidental generated files or archive
  copies as if they were a new validated release.
- Diagnostics replace the application's firmware-update page in DIAG2/DIAG3.
  Controller updating remains available through the unchanged bootloader:
  SD-root `AMBIKA.BIN`, hold S8/rightmost button at power-on.
- Claude Code (Opus 5) completed the v1.3 consolidation and release on
  2026-09-14 while Codex was rate-limited; those commits are marked
  `Co-Authored-By: Claude Opus 5`. Either agent may work here.
