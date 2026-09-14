# Single-source repository consolidation — September 14, 2026

Active folder: **KZ-Ambika**. Published repository:
**https://github.com/TijuanaKez/KZ-ambikaLive**, branch **master**.

The user authorized retaining the tested local source, signing off the stable
controller baseline and keeping completed changes committed/pushed from here on.

## Preserved inputs

- Former `KZ-ambikaLive-local`: source HEAD before consolidation
  `7b9467f73ea157feb3995ef480f4cd8101b8dcb6`, plus the working-tree changes and
  DIAG2/DIAG3 packages. It descends from the MachFour branch and contains local
  KZ modifications and the September correctness fixes.
- Former `KZ-ambikaLiva-git`: clean published checkout at
  `a471889a131bee61dc0bb99bdafbbaa9a6662583`.
- Their common ancestor is `49e8a04ad23b6fb40bf3298fc84659065f640b9d`.
- Exact pre-consolidation archives of both directories, including `.git` and
  local build files, are stored outside this repository at
  `../archives/2026-09-14-consolidation/`. Every regular file was compared against
  the archive before moving anything. Checksums are in that archive's manifest.
- The old published checkout was moved into that archive directory as
  `published-checkout`. It is historical evidence, not another active source.
- The handover now lives in `docs/`; the old parent-level filename is a symlink
  to this tracked document, avoiding two independently edited copies.

## History and dependencies

The histories have diverged. The consolidation records both parents with an
explicit **ours-strategy merge**, retaining the tested local source rather than
reintroducing old published implementations. This makes the new master a
descendant of published master, allowing a normal fast-forward push with no
forced history replacement. The older published tree remains accessible through
Git history and the preservation tag `archive-published-2026-09-14`.

The old origin pointed to MachFour. It is renamed `upstream-machfour` and is used
only for reference; the user's former `published` remote becomes `origin`.

Two obsolete gitlinks were removed:

- `avrlib`: `0836a653c6b32db963fa3dbfcc0e6ff8f68f34f8`, previously configured at
  `git://github.com/pichenettes/avril.git`.
- `tools`: `43cdf1925c0a49a95fd50b2505811c3a0b01f76a`, previously configured at
  `git://github.com/pichenettes/avril-firmware_tools.git`.

The local `avrlib` had real modified source but no independent Git metadata.
Leaving it as a gitlink would silently omit the actual build inputs from a new
clone. Its exact files are now tracked directly in this repository. The local
`tools` directory was empty; its old gitlink is preserved in history, not revived
as an unverified dependency. Legacy MIDI/SysEx packaging targets which refer to
missing `tools/hex2sysex` are outside the validated BIN build workflow.

The apparent motherboard schematic change was only a byte-identical rename from
`.sch` to `.xml`. The pre-consolidation archive preserves that state; the active
tree restores the original `.sch` filename without changing file contents.

## Ongoing workflow

Use this folder and `origin/master`. Fetch before starting, make focused changes,
run relevant checks, commit completed work and push normally. Do not force-push
or use the archived checkout as a working copy. Preserve tested firmware packages
and create new packages/tags for later hardware milestones.
