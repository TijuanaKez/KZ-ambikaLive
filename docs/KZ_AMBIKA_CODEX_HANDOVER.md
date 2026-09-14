# KZ Ambika Live --- Codex Handover

## Mission

This is a preservation-first modernization of **KZ Ambika Live**,
Carey's fork of Mutable Instruments Ambika firmware.

The immediate goal is **not** to add features or refactor the
architecture. First:

1.  Put the local source under sane Git version control without losing
    any unpublished work.
2.  Establish the relationships between the historical source trees.
3.  Identify obvious correctness / memory / undefined-behavior bugs,
    especially in KZ changes.
4.  Reproduce known historical builds where practical.
5.  Get the firmware compiling on a modern AVR toolchain while
    preserving behavior.
6.  Reach a clean, stable, reproducible baseline on the existing Ambika
    hardware.
7.  Only then begin improvements, ordered **low-hanging-fruit first**.

The **local KZ source is presumed newer/more complete than GitHub**. Do
not treat GitHub HEAD as authoritative and do not overwrite the local
tree from GitHub.

------------------------------------------------------------------------

## Current checkpoint — September 14, 2026

Resume here before following the original phase/work-order text below.

> **Note for Codex, from Claude (Opus 5), September 14, 2026.** Codex hit a usage
> limit mid-sentence, immediately after saying it would reconcile the two Git
> histories and just as Carey asked for a GitHub Release. Carey asked me to
> finish that work in your absence. Everything I did is listed under
> "Release v1.3" below and is in the Git history, signed
> `Co-Authored-By: Claude Opus 5`. I did not revisit or reverse any of your
> design decisions. Two things you should know: the docs you had written
> described the consolidation as complete when it had not yet been executed
> (branch, tag and push were all still pending), so I corrected them to match
> reality; and `scripts/build_controller_diagnostic.py` is now
> `scripts/build_controller.py --variant {release,diagnostic}`, because a
> shipping image was needed alongside the diagnostic one. `AMBIKA_DIAG3.BIN`
> rebuilds byte-identically from the renamed script, so Carey's hardware
> sign-off still applies to the artifact in the repository.

### In progress: v1.4 — deferred library loading

`kSystemVersion` is `0x14` on master. **Built and unit-tested, not yet run on
hardware.** Test packages are at `KZ-firmware_builds/test-v1.4-2026-09-14/`
(release) and `.../test-v1.4-2026-09-14-diag/` (memory screen). Flash 52,550,
static SRAM 3,829, so expect `LOW` near 20 instead of 23 — confirm that first.
Details in Phase 6.2 and the AU/VST section below. Do not tag or publish v1.4
until Carey reports hardware results.

### Release v1.3 — the stability milestone

`kSystemVersion` is now `0x13`, so the OS information page reports **v1.3**.
Units were reporting v1.2, and Carey chose to continue that numbering. The
release is explicitly a stability milestone: modern toolchain, preferences-page
corruption fixed, flash headroom recovered, no new features.

- Shipping image: `KZ-firmware_builds/release-v1.3-2026-09-14/AMBIKA.BIN`,
  **51,924 bytes flash / 3,826 bytes static SRAM**. Same source and commit as
  DIAG3, built without `-DDIAGNOSTIC_BUILD`, so it keeps the normal
  firmware-update page. It has not itself been run on hardware; DIAG3 carries the
  hardware evidence. See `STABLE_BASELINE.md`.
- Flash headroom for the coming work: the v1.2 controller BIN published on GitHub
  is 60,616 bytes, leaving only 824 bytes free. v1.3 leaves 9,516. Note that most
  of that difference comes from the `features.h` switches rather than from GCC 9,
  and the preserved local 2020 BIN was 51,724 — 200 bytes *smaller* than v1.3.
  Do not quote these as a compiler benchmark.
- SRAM is the binding constraint, not flash. 270 bytes remain and DIAG2 observed
  LOW=23. Measure before adding anything that consumes SRAM.
- Git consolidation is now actually done: `avrlib` is tracked as real files
  rather than a stale gitlink, the published history was joined with an
  ours-strategy merge, `master` carries the work, and tags `v1.3` and
  `stable-controller-2026-09-14` point at it.
- Preserved from the published repository, which the local tree lacked:
  `KZ-firmware_builds/legacy-v1.2-published/` (the v1.2 controller and voice-card
  BINs users are actually running) and `utils/ambika_program_as_text.py`.

**Carey signed off DIAG3 controller stability on September 14, 2026 and authorized
publishing the baseline and consolidating to one synchronized source.** The active
folder is now `KZ-Ambika`, remote `origin` is the user's
`TijuanaKez/KZ-ambikaLive` repository, and the working branch is `master`.
See `STABLE_BASELINE.md` for sign-off scope and `REPOSITORY_CONSOLIDATION.md` for
preservation/history details. The tag is `stable-controller-2026-09-14`.
The phase checklist below is retained as historical context; do not restart the
initial archaeology or overwrite this baseline from an older checkout.

- The authoritative `KZ-Ambika` tree has a Git preservation commit
  `c4de6a8` and subsequent correctness commits through `7b9467f`
  (synthetic preferences-control guard). Diagnostic/navigation work and the exact local avrlib are now
  included in the consolidated stable revision.
- Carey's hardware report: entering preferences `more->` corrupted the display
  and destabilized the controller before he could reach diagnostics. The old
  `/tmp/kz-ambika-diagnostic-test/AMBIKA_DIAGNOSTIC.BIN` was created September 13
  at 08:47, before the 11:02 guard commit; do not confuse it with the new image.
- **DIAG2** is packaged at
  `KZ-Ambika/KZ-firmware_builds/diagnostic-2026-09-14/`.
  Read that directory's `README.md` for changes, limitations and hardware steps.
  `AMBIKA_DIAG2.BIN` uses **51,198 bytes flash / 3,826 bytes static SRAM**,
  passing both strict limits. It opens a memory-only diagnostic screen after
  initialization, showing current RAM headroom and startup stack-paint margin.
- Carey reports DIAG2 appears stable after browsing multis, changing settings
  and general synth use. The lowest observed **LOW was 23 bytes**, with no zero
  observed so far. This is a small measured untouched-stack margin, not proof
  of adequate margin on every path; continue runtime stack investigation before
  declaring adding SRAM-consuming features. Carey subsequently signed off DIAG3
  controller stability as the baseline; this stack observation remains a budget constraint.
- DIAG2 hardware issue: `more->` on preferences did not respond to the expected
  last control. Its `OnPot` path ignored navigation controls; only main-encoder
  click/scroll navigation was implemented. The follow-up implementation uses the bottom-right
  parameter knob; Carey then reported stable operation and signed off DIAG3.
- **DIAG3** is now packaged at
  `KZ-Ambika/KZ-firmware_builds/diagnostic-2026-09-14-diag3/`:
  **51,252 bytes flash / 3,826 bytes static SRAM**. It adds direct knob navigation
  for more/back: clockwise above the centre dead band selects preferences B;
  anticlockwise below it returns to A. The thresholds are 80/47 on the 0–127
  pot scale, preventing repeated toggles during a continuous sweep. It adds no
  static SRAM. Main-encoder click/scroll is retained. DIAG3 hardware stability was
  signed off by Carey; the LOW=23 report belongs to DIAG2.
- Only the controller is to be replaced for these tests; keep the working
  voice-card firmware and controller backup. **The DIAG3 controller baseline is signed off.**
- Preferences navigation now retains more/back labels, supports clicking them,
  rejects synthetic/invalid parameter accesses, and saves settings from page B.
  Compile-time assertions check enum/table alignment and stored settings layout.
- Preserve `padding[6]`: adding two settings consumed two reserved bytes while
  retaining the original 16-byte EEPROM record. Restoring padding to eight would
  enlarge the record and shift its calculated EEPROM start address.
- Current feature switches pass the layout checks. Enabling
  `DISABLE_SEQUENCER` or `DISABLE_VERSION_MANAGER` exposes a pre-existing page
  table/enum mismatch; builds now reject it rather than shipping shifted handlers.
- RST is limited: the checked bootloader clears MCUSR before application startup.
  `RST 00` does not establish that no watchdog/brownout/external reset occurred.
- Host UI tests pass with UBSan and checked substitutes. ASan fails during macOS
  runtime initialization, including outside the sandbox. AVR disassembly and
  linked sizes were checked; hardware sign-off is recorded above and in `STABLE_BASELINE.md`.
- Reproducible packager: `scripts/build_controller.py` within the
  local tree. It archives exact source inputs and checks flash/SRAM limits.

Further audit leads found during this pass (not changed in DIAG2):

- `ParameterManager::AddressToParameterId()` truncates the NRPN table, but then
  uses sentinel values 254/255 as indexes instead of returning them; address 192
  also falls through to a table read. Trace callers and fix/test separately.
- The ordinary OS update page allows an ALL selection but still calls
  `GetVersion(active_control_)` before handling ALL, and its range includes an
  additional value above ALL. Audit port bounds before ordinary-page hardware use.
- `DISABLE_CC_MAPS` is defined in `features.h` but no consuming conditional was
  found in controller/common source. Verify each switch's actual effect rather
  than assuming its name guarantees feature removal.

------------------------------------------------------------------------

## Working Directory Layout

The parent working directory contains the active KZ repository and these
historical reference trees:

-   `ambika-PICHENETTES` --- original/upstream Ambika source.
-   `ambika-MACHFOUR` --- Mach Four's experimental work, including
    attempts to reduce build size / modernize compilation.
-   `ambikaYAM-(NoMod)` --- YAM Ambika codebase.
-   `KZ-Ambika` --- **Carey's local working KZ Ambika Live
    tree. Treat this as the primary/authoritative KZ source until proven
    otherwise.**
-   Former `KZ-ambikaLiva-git` --- archived under
    `archives/2026-09-14-consolidation/published-checkout`; no longer an active source.
-   `shruthi-1 YAM` --- Shruthi/YAM source.
-   `MI Eurorack` --- later Mutable Instruments source which may contain
    useful implementation ideas. Do not assume it is directly
    compatible.

Do not merge these trees prematurely. Their differences are evidence.

------------------------------------------------------------------------

# PHASE 0 --- Safety, Inventory and Git Archaeology

## 0.1 Do not modify source initially

Begin read-only.

Inventory:

-   repositories / `.git` metadata already present;
-   branches, tags and remotes;
-   timestamps where useful;
-   build artefacts (`.hex`, `.elf`, `.map`, EEPROM images);
-   Makefiles and toolchain assumptions;
-   untracked or generated resources;
-   differences between all relevant trees.

Do **not** run bulk formatters, modernizers or automated refactors.

## 0.2 Establish Git safely

The first practical task is to get Git sorted.

The local KZ tree contains work that may never have been pushed to
GitHub. Preserve it exactly.

Recommended outcome:

-   clone Carey's published KZ GitHub repository into
    `KZ-ambikaLiva-git`;
-   compare that clean clone against `KZ-Ambika`;
-   determine what local changes are unpublished;
-   preserve the complete local state in Git before editing it;
-   retain enough history/references that we can compare:
    -   original Ambika → YAM;
    -   YAM → KZ;
    -   published KZ → local KZ;
    -   KZ/YAM → Mach Four.

Before making source changes, produce a concise archaeology report
explaining the likely ancestry and the unpublished/local delta.

**Never reset the local tree to GitHub.**

------------------------------------------------------------------------

# PHASE 1 --- Known Historical Build Constraints

Historical notes say the original environment required:

-   Python 2.5
-   GNU Make
-   `avrdude`
-   AVR-GCC 4.3.3 historically
-   paths edited in `avrlib/makefile.mk`

Known size constraints:

-   Voice-card flash must remain below **31,744 bytes**.
-   Motherboard/controller flash must remain below **61,440 bytes**.
-   Controller RAM target: below **3,968 bytes** (4096 minus 128 bytes
    historical stack margin).

Historical controller commands:

``` sh
make -f controller/makefile bin
make -f controller/makefile size
make -f controller/makefile ramsize
make -f controller/makefile resources
```

Historical voice-card commands:

``` sh
make -f voicecard/makefile bin
make -f voicecard/makefile resources
```

Resource compiler could also be invoked with:

``` sh
python avrlib/tools/resources_compiler.py controller/resources/resources.py
python avrlib/tools/resources_compiler.py voicecard/resources/resources.py
```

A historical macOS build required changing a backslash to a forward
slash in `controller/resources.cc`.

## Compiler observations that must be investigated, not merely worked around

Carey's historical notes report:

-   AVR-GCC **4.3.3** produced a working voice-card build.
-   Later compilers could compile the voice-card but the resulting card
    could be **silent**.
-   AVR-GCC 4.3.3 could push the controller over its 61,440-byte limit.
-   AVR-GCC **4.5.1** successfully built the controller and reportedly
    saved roughly **2,700 bytes**.
-   Other historical reports found 5.4.0 could work under particular
    fixes/settings.
-   CrossPack releases found in the old environment included GCC 4.6.2,
    4.7.2 and 4.8.1.

Do not simply recreate an ancient compiler forever. The objective is to
understand **why code generation changes behavior**, then make the
source correct and stable on a current AVR toolchain.

------------------------------------------------------------------------

# PHASE 2 --- Correctness / Memory / UB Audit

Before feature work, audit the entire KZ fork, concentrating especially
on Carey's changes and code paths affected by them.

This is an embedded AVR project; "memory bugs" includes much more than
heap leaks. Look specifically for:

-   out-of-bounds reads/writes;
-   unsafe pointer arithmetic;
-   old C/C++ sequence-point / evaluation-order assumptions;
-   overlapping source/destination structures;
-   `PROGMEM` addressing/access mistakes;
-   incorrect pointer/address-space assumptions;
-   stack exhaustion;
-   SRAM overrun;
-   ISR/main-loop shared-state problems;
-   missing/incorrect `volatile`;
-   atomicity problems on multi-byte values;
-   integer promotion/sign/overflow/wrap assumptions;
-   array-size/index bugs;
-   aliasing violations;
-   lifetime issues;
-   struct packing/layout assumptions;
-   uninitialized data;
-   buffer increment expressions whose meaning changed or was always
    undefined;
-   compiler-specific AVR behavior;
-   optimization-sensitive code;
-   timing/cycle-budget changes in real-time audio code.

Do not classify unusual code as a bug merely because it looks old or
ugly. Mutable firmware is heavily optimized for constrained AVR
hardware. Explain the actual failure mode and generated-code
implications before replacing deliberate tricks.

## Previously identified suspicious/fixed areas

Historical modernization notes specifically mention:

### Voice card optimization

Add `-O2` to voice-card `CPPFLAGS` as part of an earlier successful
attempt. Verify why it was required and whether it remains appropriate
with the current compiler.

### `voice.cc`

There was an issue involving an **overlapping data structure and bytes
buffer** in `voicecard/voice.cc`. Locate the exact code/history and
determine the standards/aliasing/overlap issue.

### Formant interpolation

Historical warning/fix:

``` cpp
for (uint8_t i = 0; i < 4; ++i)
```

was believed to need:

``` cpp
for (uint8_t i = 0; i < 3; ++i)
```

Verify this against the actual array/data semantics rather than applying
blindly.

### `sub_oscillator.h`

Historical code:

``` cpp
*buffer++ = U8Mix(*buffer, v, mix_gain, sub_gain);
```

was changed to:

``` cpp
*buffer = U8Mix(*buffer, v, mix_gain, sub_gain);
++buffer;
```

Investigate the sequencing/evaluation issue and verify all analogous
expressions.

### `transient_generator.h`

Likewise:

``` cpp
*buffer++ = U8Mix(*buffer, value, amplitude);
```

was changed to:

``` cpp
*buffer = U8Mix(*buffer, value, amplitude);
++buffer;
```

Search for the same pattern elsewhere.

## Audit deliverable

Before broad changes, produce a report classifying findings roughly as:

-   **Definite correctness bug / UB**
-   **Highly suspicious / optimization-sensitive**
-   **Modern compiler incompatibility**
-   **Intentional embedded optimization**
-   **Style/legacy issue only**
-   **Needs hardware verification**

For every proposed fix, explain why it preserves intended behavior.

------------------------------------------------------------------------

# PHASE 3 --- Establish Reproducible Builds

Work incrementally.

## 3.1 Historical reference build

Where practical, reproduce at least one known-good historical build
using the old toolchain or a contained equivalent.

Capture:

-   compiler versions;
-   complete flags;
-   flash size;
-   SRAM/static RAM size;
-   `.hex`;
-   `.elf`;
-   `.map`;
-   EEPROM/resource outputs;
-   warnings.

These become reference artefacts.

## 3.2 Modern toolchain build

Move to a current supported AVR toolchain in controlled steps.

Goals:

-   controller builds;
-   voice card builds;
-   bootloader/resource steps build;
-   size limits remain satisfied;
-   no silent voice-card regression;
-   warnings are understood;
-   no blanket warning suppression simply to achieve green output.

Prefer small commits with one logical compiler/correctness fix each.

Do not combine modernization with feature changes.

## 3.3 Host-side testing where practical

Extract/test pure logic on the host when feasible:

-   oscillators;
-   lookup/index logic;
-   modulation math;
-   SysEx encode/decode;
-   parameter mappings;
-   interpolation;
-   resource transforms.

Use modern sanitizers/static analysis for host-compatible pieces where
useful.

Do not pretend host testing validates AVR timing, `PROGMEM`, peripherals
or hardware-specific behavior.

## 3.4 Behavior comparison

Where feasible compare old/reference and modern builds numerically:

-   oscillator samples;
-   envelopes/modulators;
-   parameter conversion;
-   MIDI/SysEx behavior;
-   lookup outputs;
-   resource data.

The first modern build should intentionally be **boring**:
behavior-identical or as close as reasonably demonstrable.

------------------------------------------------------------------------

# PHASE 4 --- Hardware Validation

Only after the build is understood should firmware be flashed.

## Voice-card programming --- historical known procedure

Target MCU: **ATmega328P**, programmer: **USBasp**.

Historical fuse/lock setup:

``` sh
./bin/avrdude -C etc/avrdude.conf -B 100 -V -p m328p -c usbAsp -P usb \
  -e -u \
  -U efuse:w:0xfd:m \
  -U hfuse:w:0xde:m \
  -U lfuse:w:0xff:m \
  -U lock:w:0x2f:m
```

Historical programming:

``` sh
./bin/avrdude -C etc/avrdude.conf -B 1 -V -p m328p -c usbAsp -P usb \
  -U flash:w:ambika_voicecard.hex:i \
  -U flash:w:ambika_voicecard_boot.hex:i \
  -U eeprom:w:ambika_voicecard_eeprom_golden.hex:i \
  -U lock:w:0x2f:m
```

A historical CrossPack path was:

``` text
/Volumes/iMac HD/usr/local/CrossPack-AVR-20121207/bin
```

Do not assume these commands are appropriate for a modern environment
without checking them, but preserve them as the known historical
programming recipe.

------------------------------------------------------------------------

# PHASE 5 --- Tag the Stable Baseline

Once:

-   Git history is sane;
-   the local/published delta is preserved;
-   obvious correctness/UB issues are resolved;
-   controller and voice card compile reproducibly on the modern
    toolchain;
-   size limits are understood;
-   real hardware appears stable and behaviorally correct;

create/tag a clear **modernized KZ Ambika Live baseline**.

Only after this point begin feature development.

------------------------------------------------------------------------

# PHASE 6 --- Improvements: Low-Hanging Fruit First

The first generation of improvements must run on **Carey's existing
Ambika hardware**.

Before implementing any item:

1.  measure current flash/SRAM/cycle budget;
2.  estimate the cost;
3.  identify removable features/data Carey does not use;
4.  implement one major change at a time;
5.  measure before/after;
6.  retain an easy route back to the stable baseline.

Candidate directions, not immediate requirements:

## 6.1 Reclaim space

Carey rarely uses much of the existing wavetable library. Investigate
which tables/features consume meaningful flash and whether removing
unused material creates enough room for more valuable synthesis
improvements.

Do not delete content before documenting exactly what would be lost.

## 6.2 Fast preset browsing: name cache and delayed loading

Requested by Carey on September 14, 2026 as a low-hanging-fruit candidate
after firmware stabilization, on the existing hardware.

### Stage 1 implemented, September 14, 2026 (v1.4, awaiting hardware test)

The delayed load is done and **no cache was needed for it**. Measurements that
shaped this, all verified in source rather than assumed:

- Presets are one file per slot, `/PATCH/BANK/A/000.PAT` and so on, over banks
  A-Z and slots 0-127, so 3,328 slots per object type.
- The bottleneck was not name reading. `Library::OnIncrement` ran the full
  `storage.Load()` on every detent: whole RIFF parse, voice-card parameter push,
  and a snapshot **write** to the card when the patch had unsaved edits. That is
  now deferred; scrolling calls `LoadName()`, which skips the object chunks and
  the snapshot.
- A RAM cache is impossible: 270 bytes free, one bank of names is 2,048.
- EEPROM cannot hold one either, and the binding reason is capacity, not write
  endurance. The stored multi occupies 6 x (PartData 112 + 1 + Patch 84 + 1) +
  MultiData 56 + 1 = **1,245 bytes** of the 2,048, plus 16 bytes of settings and
  the firmware flag. About 786 bytes remain, roughly 49 names.
- The delay is a system setting (`PRM_SYSTEM_BROWSE_LOAD_DELAY`, shown as `ldly`
  on preferences page B) in units of 10 ms, so it can be tuned on the unit
  without reflashing. 0 restores the old behaviour and is the default on
  settings that have not been re-saved.
- Cost: 3 bytes of static SRAM for the timer, taking the expected `LOW` from 23
  to about 20. **Confirm on hardware before building anything else here.**

### Stage 2, if Stage 1 is not enough

Only if browsing still feels slow after the deferral. One flat index file per
bank, `NAMES.IDX`, 128 x 16 bytes, indexed by slot; browsing becomes a seek and
a 16-byte read. `ffconf.h` sets `_FS_TINY 1`, so FatFs keeps a single shared
512-byte sector buffer and **32 names live in one sector** — scrolling inside a
32-slot window would cost no SD reads at all.

Do not add a second persistent `File` object for it: `sizeof(FIL)` is 32 and the
stack has nothing like that to spare. Reuse `Storage::file_`, which is already
opened and closed constantly and is not held open while browsing.

Staleness is free to handle: the deferred load opens the real file anyway, so
compare the name then and correct the entry.

Carey reports that Ambika's sluggish preset browsing comes from accessing
individual preset files on the SD card as the selection changes. Verify
and time the current browsing/name-read/full-load paths before designing
the replacement.

Desired behavior:

- Scroll rapidly through cached preset names without opening each preset
  file or loading its synthesis data on every encoder movement.
- Keep the currently loaded preset active while browsing. Track the
  highlighted candidate separately from the loaded preset.
- Load only the latest selected preset after approximately **500 ms without
  another selection change**. Restart the delay on each change and cancel
  obsolete pending loads, including when leaving the browser. Use a
  nonblocking timer so MIDI, audio control and the UI remain responsive.
- Use the same name index for the future AU/VST editor's preset-name sync.

Cache design and storage are to be measured, not assumed:

- Keep a compact index keyed by storage-object type, bank and slot, with
  names and enough information to distinguish occupied and empty slots.
  Define coverage for patches, programs, sequences and multis against the
  actual library layout.
- Evaluate available onboard EEPROM and/or an SD-card index file for
  capacity, read latency, write endurance and update cost. Do not assume
  spare EEPROM or sufficient SRAM exists. Budget any small RAM window or
  bank buffer against the controller's **3,968-byte static SRAM limit** and
  measured stack needs.
- An SD-backed cache can still require SD reads; its purpose is to avoid
  repeated per-preset file opens/directory work and full preset loads.
  Benchmark whether compact indexed reads plus a small RAM buffer deliver
  the desired scrolling speed.
- Time a complete cache build on realistic full cards. If it is fast enough
  to add only an acceptable number of milliseconds to startup, rebuild at
  startup. Otherwise provide a manual **rebuild preset-name cache** option
  in settings, with appropriate progress feedback.
- After a successful preset save or rename, update just that cache entry.
  Account for deletes, copies and other library operations that change names
  or occupancy. Preset files remain authoritative; the cache is rebuildable.
- Detect or safely handle missing/stale caches, card swaps, files edited
  externally and interrupted updates. Avoid showing names from a previous
  card or leaving a cache entry claiming a save succeeded when it did not.

Validation should measure name-scroll latency, the number of full loads
during rapid scrolling, startup/rebuild time, single-entry update cost,
flash/SRAM/stack usage and responsiveness during loads. A rapid scroll
followed by a pause should produce one load of the final selection, not a
queue of intermediate loads.

This is shared groundwork for the JUCE AU/VST editor below: expose cached
names through a documented firmware protocol so the editor need not load
every preset to discover its name. Keep cache revision/invalidation and
incremental name changes in mind when designing that protocol.

## 6.3 Oscillator quality

Carey predominantly uses analog-style oscillator shapes: saw,
square/pulse, sine and triangle.

YAM introduced PolyBLEP-style improvements. Investigate, benchmark and
compare alternatives that fit the existing AVR:

-   current YAM/KZ PolyBLEP implementation;
-   improved/higher-order BLEP approaches where practical;
-   minBLEP where memory/cycles permit;
-   DPW or other efficient band-limited techniques where appropriate;
-   improved PWM behavior.

Judge candidates by:

-   audible aliasing;
-   alias-energy measurements;
-   CPU cycles/sample;
-   flash;
-   SRAM;
-   behavior across the playable pitch range.

Do not chase mathematical purity at the expense of the Ambika's
character or real-time reliability.

## 6.4 Controlled analog-style instability

Carey values the Ambika's ability to create smooth, imperfect modulation
using noise processed through lag/math modulation.

Potential future work includes richer low-frequency stochastic sources /
filtered noise / correlated drift suitable for subtle modulation of:

-   oscillator pitch;
-   filter cutoff;
-   other synthesis parameters.

The objective is **musically useful instability**, not hiss or
indiscriminate white noise.

## 6.5 Preserve character

Do not assume "higher resolution", "cleaner", or "more numerically
perfect" is automatically better.

Carey previously sold a Novation Peak partly because its very
clean/stable digital oscillator architecture felt sterile despite its
technical capability. The design target is a warm, dusty, organic hybrid
synth, not merely minimum THD/aliasing at all costs.

------------------------------------------------------------------------

# FUTURE PROJECT --- Shruthi XT as Ambika Control Surface

Carey also built a **Shruthi XT**, which is currently underused.

Longer-term objective:

-   create a controller-only Shruthi XT firmware;
-   ignore/disable Shruthi synthesis/audio;
-   use its extensive knobs/buttons/display as a physical Ambika control
    panel.

An earlier KZ approach expanded MIDI CC mapping.

A more interesting future approach is to investigate a direct wired
connection using the Ambika's expansion/interface facilities. Carey
recalls this may involve I²C, but **verify the actual hardware and
protocol from schematics/source before assuming this**.

Possible architecture:

``` text
Shruthi XT control surface
        │
        │ direct digital control link
        ▼
      Ambika
```

This is post-stabilization work.

------------------------------------------------------------------------

# FUTURE PROJECT --- JUCE AudioUnit / Editor

Carey has an editor project at:

``` text
/Volumes/iMac Data/Nextcloud/JUCE Projects/Shruthi And Ambika Editors
```

It is based on/forked from the **IXOS Shruthi/Ambika editor**,
implemented in **JUCE**.

Carey previously:

-   updated it to compile against newer JUCE versions;
-   attempted improved two-way communication;
-   specifically attempted to retrieve/browse Ambika patch names from
    the hardware;
-   did not get patch-name communication working reliably.

Treat this as a separate secondary codebase. Do not let it distract from
firmware stabilization.

## Relevant Ambika storage/SysEx observations

Patch names are not simply part of the patch settings data structure.
Historical notes indicate `Library::name_` contains the name of the
currently active storage object.

The existing Ambika SysEx framework includes:

-   Mutable manufacturer ID: `00 21 02`
-   Ambika product ID: `00 04`
-   structured data requests;
-   nibblized payloads/checksums;
-   a PEEK/random-memory-read facility.

Carey experimented with a proposed patch-name request around command
`0x16`.

A clean future solution may involve adding an explicit, documented
firmware-side SysEx command/API for:

-   requesting patch/program/multi names;
-   browsing banks/slots;
-   possibly retrieving current object names/state;
-   robust two-way editor synchronization.

Use the shared preset-name cache planned in Phase 6.2 as the source for
AU/VST name browsing/synchronization, with full-list retrieval and a way to
identify subsequent name changes or cache rebuilds. Reading names through
the editor should not require loading each preset into the synthesizer.

**The Ambika side of this now works (v1.4, September 14, 2026).**
`SYSEX_REQUEST_PATCH_NAME` (0x16), `_SEQUENCE_NAME` (0x17), `_PROGRAM_NAME`
(0x18) and `_MULTI_NAME` (0x1a) take the bank in the argument byte and the slot
in data[0], and reply with the same command/argument plus the 16-byte name,
nibblized like every other Ambika payload.

The scaffolding existed but had never worked: `Load()` fills a caller-supplied
buffer and never sets `location.name`, so the handler passed a null pointer to
`SysExSendRaw`, which nibblized 16 bytes read from **address 0** — the AVR
register file — out over MIDI. It now supplies a buffer from the scratch half of
`tmp_buffer_`, rejects banks outside A-Z, and always replies, using a blank name
for an empty or invalid slot so the editor can distinguish "empty" from "no
answer". Note that `buffer_` is the shared FatFs sector buffer, so the slot byte
must be read before `Load()` touches the card.

Not built, deliberately: a bulk range request. One request per name means 128
round trips for a bank, which is acceptable for background population. A bulk
reply buffer would cost static SRAM the controller does not have until the stack
margin improves. Revisit together with Stage 2 above, which produces exactly the
index a bulk reply would want to stream from.

Do not rely on arbitrary RAM PEEK as the long-term plugin API if a
proper protocol extension is feasible.

------------------------------------------------------------------------

# Voice card v2 --- the first deliberate break with Ambika compatibility

Planned September 14, 2026. See **`VOICECARD_V2_PLAN.md`** for the full
architecture plan; it supersedes the sketch below for anything oscillator- or
firmware-related. Headlines:

-   `master` stays the compatible v1 line. v2 work lives on `v2-voicecard`.
    Tag `v1.4` before starting, and that tag is the way back.
-   The voice card has **1,524 bytes of flash free** out of 31,744. Removing the
    wavetable and bandlimited-zone tables reclaims about **15,700**, which is
    what makes new oscillator types possible at all.
-   The binding constraint is CPU, not flash: **510 cycles per sample** at
    39.2 kHz, and it has not been measured yet. `TIMING_CODE` in `voicecard.cc`
    already has the instrumentation. Measure before designing.
-   SRAM: 1,072 of 2,048 used, **976 free**. This decides whether a
    Karplus-Strong delay line is possible, and therefore whether the signal path
    can widen from its current 8 bits.
-   BRAIDS is STM32/32-bit; no code ports. The portable part is which algorithms
    earn their cycles. Proposed starting set: detuned multi-saw, hard sync,
    wavefolding, plucked string.
-   Old patches convert in firmware on load, keyed off a new RIFF structure ID.

------------------------------------------------------------------------

# FUTURE PROJECT --- Voice Card Hardware

Do **not** begin this until the existing Ambika hardware/firmware has
been pushed as far as is worthwhile.

Eventually investigate improved/reimagined Ambika-compatible voice
cards.

Design philosophy discussed so far:

-   preserve the Ambika motherboard/UI/voice-allocation platform;
-   potentially improve oscillator MCU/DAC architecture;
-   retain real analog filtering;
-   avoid producing a sterile "perfect" hybrid;
-   deliberate per-voice variation can be desirable;
-   modern SMD components are fine for infrastructure where they do not
    meaningfully contribute character;
-   intentionally nonlinear/characterful sections should be identified
    empirically rather than assuming old/through-hole automatically
    sounds better.

One particularly interesting direction is comparing the **Shruthi filter
implementations with the Ambika equivalents** and determining exactly
what circuitry was simplified for cost/space when six complete voice
cards had to fit inside Ambika.

Carey perceives some Ambika filter resonance behavior as moving too
quickly from mild filtering into a narrow/whistling resonance rather
than producing the broad, vowel-like resonant region he likes in classic
four-pole filters.

Future work may include a "no-compromise" Ambika-format filter/voice
card influenced by Shruthi designs.

Again: **not Phase 1.**

------------------------------------------------------------------------

# Engineering Principles

## Preserve before improving

Never destroy historical information to make the tree cleaner.

## Measure before optimizing

For every meaningful embedded optimization capture, where applicable:

-   flash bytes;
-   SRAM bytes;
-   stack implications;
-   cycles/timing;
-   audio/behavior difference.

## Explain strange code before replacing it

This firmware was written for extremely constrained hardware.
Compact/ugly code may be intentional.

### Enum sentinels, table sizes and KZ extensions

Carey notes that Emilie's original code often uses compact C-style
techniques to track memory addresses, sizes and boundaries. A final enum
member such as `UNIT_LAST` may supply a count or an exclusive upper bound
for arrays, lookup tables, loops and dispatch code. This is a value used
by the program, not the enum type's byte size; verify the actual enum
values and indexing convention before treating it as a count.

KZ added options and extended enums. When auditing these changes, trace
every affected sentinel and its consumers together: array/table lengths,
parameter/resource IDs, page ranges, handler indexes and persisted values.
Check for hard-coded bounds and conditional compilation that could leave
an enum and its associated table out of step. Preserve intentional fast
indexing only after verifying its bounds. Treat this as an audit priority,
not evidence that every extended enum is faulty.

### Feature switches and build identity

Carey created `KZ-Ambika/common/features.h` to quickly enable or
disable modifications and features, including options that change BIN
size. Inspect this file before building or comparing firmware; record its
exact configuration together with command-line defines, compiler/flags,
flash size and static SRAM usage. Feature switches can also alter the code
paths and enum/table relationships that need validation. Do not silently
toggle them just to fit a size budget.

As inspected on September 14, 2026, the active defines are:

- `DISABLE_CARD_INFO_PAGE`
- `DIRTY_CC_LOOKUP`
- `DISABLE_CC_MAPS`
- `DISABLE_LAUNCHKEY_MODE`
- `DISABLE_PART_MUTES`
- `POLIVOKS_FILTERBOARD`

The file also has commented-out switches for version management, ragas,
groove templates, sequencing, arpeggiation, LFO memory reduction and a
bandlimited triangle. Re-read the file for future builds; this list is a
dated snapshot. `DIAGNOSTIC_BUILD` was supplied on the compiler command
line for the recovered diagnostic build, rather than defined in this file.

## Separate correctness from modernization

A standards/UB fix should ideally be its own change. A toolchain change
should be distinguishable from a feature change.

## Separate modernization from synthesis improvements

We need a trustworthy baseline before evaluating whether a new
oscillator algorithm sounds better.

## Existing hardware first

The near-term product is a significantly improved KZ Ambika running on
the Ambika Carey already owns.

------------------------------------------------------------------------

# Initial Codex Work Order

Start here and proceed in this order.

### Task 1 --- Git / repository archaeology

Read-only first.

Inspect all source trees and Git metadata. Clone the published KZ
repository into the empty Git folder if repository/network access is
available. Compare it against `KZ-Ambika`.

Determine:

-   ancestry;
-   unpublished local changes;
-   relationship to YAM;
-   relationship to original Ambika;
-   relevant Mach Four changes.

Propose the safest Git structure before modifying source.

### Task 2 --- Preserve the local KZ state

Once the archaeology is understood, put the authoritative local KZ state
under proper version control without losing or overwriting anything.

Create an identifiable preservation point before modernization.

### Task 3 --- Audit obvious bugs

Perform the correctness/UB/memory audit described above.

Prioritize issues that could explain:

-   modern-compiler instability;
-   silent voice-card builds;
-   random corruption;
-   optimization-dependent behavior.

Report before making broad changes.

### Task 4 --- Reproduce a reference build

Establish a known historical build where feasible and record
sizes/artefacts/toolchain.

### Task 5 --- Modern compiler build

Get controller and voice-card builds working on a modern AVR toolchain
with minimal behavior-preserving changes.

Work incrementally. No feature development yet.

### Task 6 --- Validate

Run static/host tests where useful, compare behavior/reference outputs,
then prepare a controlled hardware test build.

### STOP POINT

When the modern build is clean, reproducible and ready for hardware
validation, **stop and report back** before beginning KZ Next feature
work.

------------------------------------------------------------------------

# First Response Requested From Codex

Before changing source, return:

1.  an inventory of the source trees and detected Git history;
2.  the likely ancestry/relationships;
3.  the difference between published KZ and local KZ;
4.  the historical/current build-system assessment;
5.  the highest-risk correctness/UB findings;
6.  a proposed Git preservation/migration plan;
7.  a proposed minimal sequence of commits to reach a modern build;
8.  anything that requires Carey's clarification.

Do not begin feature development.
