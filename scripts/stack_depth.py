#!/usr/bin/env python3
"""Find the deepest call chain in an AVR build, and what it costs in stack.

LOW on the diagnostic screen says the stack got within N bytes of static data,
but not which path did it. This reconstructs the call graph from the ELF
disassembly, attaches the per-function frame sizes GCC reports with
-fstack-usage, and reports the worst chain.

Build the inputs first (LTO suppresses .su files, so this needs -fno-lto, which
means the numbers are indicative of the shipped LTO build rather than exact):

  make -f controller/makefile all BUILD_ROOT=<dir>/ \
      EXTRA_FLAGS="-fstack-usage -fno-lto" ...
  python3 scripts/stack_depth.py <dir>/ambika_controller

Interrupt handlers are reported separately: their cost lands on top of whatever
was running, so the true worst case is the deepest chain plus the deepest ISR.
"""
import argparse
from pathlib import Path
import re
import subprocess
import sys

# avr-objdump puts the resolved target in a trailing comment:
#     call\t0xd670\t; 0xd670 <__tablejump2__>
CALL = re.compile(r"\b(?:call|rcall)\b.*?<([^>+]+)")
LABEL = re.compile(r"^([0-9a-f]+) <([^>]+)>:")
# AVR pushes a 2-byte return address per call on parts with <= 128 KB of flash.
RETURN_ADDRESS_BYTES = 2


def frames(build_dir):
    """Map demangled-ish function name -> frame bytes, from GCC's .su files."""
    out = {}
    for su in Path(build_dir).parent.rglob("*.su"):
        for line in su.read_text().splitlines():
            parts = line.split("\t")
            if len(parts) < 2:
                continue
            location, size = parts[0], parts[1]
            name = location.rsplit(":", 1)[-1]
            # C++ entries carry the full signature; the symbol is the last word
            # before the argument list.
            if "(" in name:
                name = name.split("(")[0].split()[-1].split("::")[-1]
            out[name] = max(out.get(name, 0), int(size))
    return out


def graph(elf, objdump):
    text = subprocess.run([objdump, "-d", str(elf)], capture_output=True, text=True).stdout
    edges, current = {}, None
    for line in text.splitlines():
        label = LABEL.match(line)
        if label:
            current = label.group(2)
            edges.setdefault(current, set())
            continue
        if current:
            call = CALL.search(line)
            if call:
                edges[current].add(call.group(1))
    return edges


def deepest(edges, cost, root, seen=None, memo=None):
    """Longest chain from root. Recursion is cut and flagged rather than looped."""
    seen = seen or ()
    memo = memo if memo is not None else {}
    if root in seen:
        return 0, [f"{root} (RECURSION)"]
    if root in memo:
        return memo[root]
    best, path = 0, []
    for callee in sorted(edges.get(root, ())):
        depth, sub = deepest(edges, cost, callee, seen + (root,), memo)
        if depth > best:
            best, path = depth, sub
    total = cost(root) + RETURN_ADDRESS_BYTES + best
    result = (total, [f"{root} [{cost(root)}]"] + path)
    if root not in seen:
        memo[root] = result
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("target", help="Build stem, e.g. <dir>/ambika_controller")
    parser.add_argument("--objdump", default="avr-objdump")
    parser.add_argument("--top", type=int, default=6)
    args = parser.parse_args()

    elf = Path(args.target).with_suffix(".elf")
    if not elf.exists():
        raise SystemExit(f"No ELF at {elf}")
    sizes = frames(Path(args.target))
    if not sizes:
        raise SystemExit("No .su files found. Rebuild with -fstack-usage -fno-lto.")
    edges = graph(elf, args.objdump)

    def cost(name):
        key = name.split("::")[-1]
        return sizes.get(key, sizes.get(name, 0))

    # The UI dispatches page handlers through PROGMEM function pointers, and
    # oscillators through fn_table, so a static call graph cannot follow the
    # edge from the event loop into a handler. Treat every function as a
    # possible entry point instead and rank them; the event loop's own frames
    # sit on top of whichever handler it reaches.
    isrs = sorted(n for n in edges if "vect" in n and n != "__vectors")
    print(f"{len(edges)} functions, {len(sizes)} with a known frame")
    print("Indirect dispatch means every function is treated as a possible "
          "entry point.\n")

    ranked = sorted(((deepest(edges, cost, n)[0], n) for n in edges), reverse=True)
    shown = 0
    for total, name in ranked:
        if "vect" in name:
            continue
        _, path = deepest(edges, cost, name)
        print(f"=== {name} -> {total} bytes")
        for step in path[:args.top]:
            print(f"      {step}")
        if len(path) > args.top:
            print(f"      ... {len(path) - args.top} more frames")
        print()
        shown += 1
        if shown == 3:
            break

    worst_chain = max((t for t, n in ranked if "vect" not in n), default=0)
    isr_total = max((deepest(edges, cost, i)[0] for i in isrs), default=0)
    print(f"deepest chain {worst_chain} + deepest ISR {isr_total} "
          f"= {worst_chain + isr_total} bytes")
    print("Functions with no .su entry count as 0, so this is a LOWER bound,")
    print("and -fno-lto shifts inlining, so treat it as indicative not exact.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
