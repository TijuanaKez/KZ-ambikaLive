#!/usr/bin/env python3
"""Count which oscillator shapes a real Ambika library actually uses.

Walks an SD card (or any directory) of RIFF patch/program files, extracts the
oscillator shape bytes from every Patch chunk, and reports the histogram. The
shape names are parsed out of common/patch.h so this cannot drift from the
firmware.

Written for the voice card v2 planning question: which shapes may be retired,
and what the dead wavetable slots should fall back to.

  python3 utils/survey_library.py /Volumes/AMBIKA
  python3 utils/survey_library.py /Volumes/AMBIKA --bank A
"""
import argparse
from collections import Counter
from pathlib import Path
import re
import struct
import sys

# RIFF object-chunk structure IDs, per Storage::Save in controller/storage.cc.
STRUCTURE_PATCH = 1
# Byte offsets inside Patch::Parameters, per PatchParameter in common/patch.h.
OSC1_SHAPE, OSC2_SHAPE, SUB_SHAPE = 0, 4, 11


def shape_names(root):
    """Parse the active OscillatorAlgorithm enum out of common/patch.h."""
    text = (root / "common/patch.h").read_text()
    # The file keeps the original Ambika enum commented out above the live one;
    # take the last definition, which is the one that compiles.
    blocks = re.findall(r"enum OscillatorAlgorithm\s*:\s*uint8_t\s*\{(.*?)\}", text, re.S)
    if not blocks:
        raise SystemExit("Could not find OscillatorAlgorithm in common/patch.h")
    names, value = {}, 0
    for line in blocks[-1].splitlines():
        line = re.sub(r"//.*", "", line).strip().rstrip(",").strip()
        if not line:
            continue
        if "=" in line:
            name, expr = (part.strip() for part in line.split("=", 1))
            # Entries like WAVEFORM_WAVETABLE_16 = WAVEFORM_WAVETABLE_1 - 1 + 16
            expr = re.sub(r"WAVEFORM_\w+",
                          lambda m: str(names.get(m.group(0), 0)), expr)
            value = eval(expr, {"__builtins__": {}}, {})  # noqa: S307 - fixed input
        names[line.split("=")[0].strip()] = value
        value += 1
    by_value = {v: k for k, v in names.items()}
    # WAVETABLE_2..15 are implicit between WAVETABLE_1 and the explicit _16.
    first, last = names.get("WAVEFORM_WAVETABLE_1"), names.get("WAVEFORM_WAVETABLE_16")
    if first is not None and last is not None:
        for i, v in enumerate(range(first, last + 1), start=1):
            by_value[v] = f"WAVEFORM_WAVETABLE_{i}"
    by_value.pop(names.get("WAVEFORM_LAST"), None)
    return by_value


def patches_in(path):
    """Yield the payload of every Patch object chunk in a RIFF file."""
    data = path.read_bytes()
    if len(data) < 12 or data[0:4] != b"RIFF" or data[8:12] != b"MBKS":
        return
    offset = 12
    while offset + 8 <= len(data):
        tag = data[offset:offset + 4]
        (size,) = struct.unpack("<I", data[offset + 4:offset + 8])
        body = data[offset + 8:offset + 8 + size]
        if tag == b"obj " and len(body) >= 4 and body[0] == STRUCTURE_PATCH:
            yield body[4:]
        offset += 8 + size


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("library", type=Path, help="SD card root, or any directory of banks")
    parser.add_argument("--bank", help="Only this bank letter, e.g. A")
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()

    names = shape_names(args.repo)
    # Programs store a Patch chunk too. The extension is the object name
    # truncated to three characters, so PROGRAM becomes .PRO. Autobackups have
    # their first extension character replaced by '~' and are skipped.
    files = [p for p in sorted(args.library.rglob("*"))
             if p.is_file() and p.suffix.upper() in {".PAT", ".PRO"}
             and not p.name.startswith(".")]
    if args.bank:
        want = f"/BANK/{args.bank.upper()}/"
        files = [p for p in files if want in str(p).upper()]
    if not files:
        raise SystemExit("No .PAT or .PRG files found")

    wavetable_values = {v for v, n in names.items()
                        if "WAVETABLE" in n or "WAVEQUENCE" in n}
    counts, per_slot, unreadable, patch_count, affected = Counter(), Counter(), 0, 0, 0
    for path in files:
        try:
            found = list(patches_in(path))
        except OSError:
            unreadable += 1
            continue
        for payload in found:
            patch_count += 1
            shapes = []
            for label, offset in (("osc1", OSC1_SHAPE), ("osc2", OSC2_SHAPE)):
                if offset < len(payload):
                    counts[payload[offset]] += 1
                    per_slot[(label, payload[offset])] += 1
                    shapes.append(payload[offset])
            if any(s in wavetable_values for s in shapes):
                affected += 1

    print(f"{len(files)} files, {patch_count} patch chunks, {sum(counts.values())} oscillators")
    if unreadable:
        print(f"{unreadable} files could not be read")
    print()
    total = sum(counts.values()) or 1
    width = max((len(names.get(v, "?")) for v in counts), default=10)
    for value, count in counts.most_common():
        name = names.get(value, f"<unknown {value}>")
        bar = "#" * max(1, round(40 * count / max(counts.values())))
        print(f"{value:3d}  {name:<{width}}  {count:5d}  {100 * count / total:5.1f}%  {bar}")

    wavetable = {v for v, n in names.items()
                 if "WAVETABLE" in n or "WAVEQUENCE" in n}
    retiring = sum(c for v, c in counts.items() if v in wavetable)
    print()
    print(f"Wavetable / wavequence oscillators: {retiring} of {total} "
          f"({100 * retiring / total:.1f}%)")
    # The number that actually matters: a patch is affected if ANY of its
    # oscillators uses a retiring shape.
    if patch_count:
        print(f"Patches affected if wavetables are removed: {affected} of "
              f"{patch_count} ({100 * affected / patch_count:.1f}%) -- "
              f"{patch_count - affected} need no conversion at all")
    print(f"Distinct shapes in use: {len(counts)} of {len(names)}")
    unused = sorted(v for v in names if v not in counts)
    if unused:
        print("Never used: " + ", ".join(names[v] for v in unused))
    return 0


if __name__ == "__main__":
    sys.exit(main())
