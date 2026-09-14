#!/usr/bin/env python3
"""Build/package a controller image, enforcing both memory limits.

Variants:
  release      Shipping firmware. Keeps the SD firmware-update page. BIN is
               named AMBIKA.BIN so it can be copied straight to a card root.
  diagnostic   DIAG3 memory-instrumented image. Boots into the RAM/LOW screen
               and replaces the firmware-update page.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import tempfile

VARIANTS = {
    "release": {
        "defines": "-DDISABLE_DEFAULT_UART_RX_ISR",
        "bin_name": "AMBIKA.BIN",
        "label": "KZ Ambika Live controller",
        "require_no_dynamic_allocation": False,
    },
    "diagnostic": {
        "defines": "-DDISABLE_DEFAULT_UART_RX_ISR -DDIAGNOSTIC_BUILD",
        "bin_name": "AMBIKA_DIAG3.BIN",
        "label": "KZ DIAG3",
        # Stack painting is only valid with a static memory layout.
        "require_no_dynamic_allocation": True,
    },
}

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("output", type=Path, help="New directory for reviewed build artifacts")
parser.add_argument("--variant", choices=sorted(VARIANTS), default="release")
args = parser.parse_args()
variant = VARIANTS[args.variant]
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=False)

def tool(name):
    found = shutil.which(name)
    if not found:
        raise SystemExit(f"Required tool not found: {name}")
    return found

def capture(command):
    return subprocess.check_output(command, cwd=root, text=True)

gcc = tool("avr-gcc")
objcopy = tool("avr-objcopy")
size = tool("avr-size")
nm = tool("avr-nm")
defines = variant["defines"]
with tempfile.TemporaryDirectory(prefix=f"kz-{args.variant}-") as directory:
    build = Path(directory)
    command = [
        tool("make"), "-f", "controller/makefile", "bin", "all",
        f"BUILD_ROOT={build}/",
        f"AVRLIB_TOOLS_PATH={Path(gcc).parent}/",
        f"AVRLIB_TOOLS_PATH_LEGACY={Path(objcopy).parent}/",
        f"EXTRA_DEFINES={defines}",
        f"EXTRA_LD_FLAGS=,-Map,{build}/controller.map",
    ]
    with (output / "build.log").open("w") as log:
        subprocess.run(command, cwd=root, stdout=log, stderr=subprocess.STDOUT, check=True)
    target = build / "ambika_controller/ambika_controller"
    section_text = capture([size, "-A", str(target.with_suffix(".elf"))])
    sections = {name: int(count) for name, count in
                re.findall(r"^(\.\S+)\s+(\d+)\s+\d+", section_text, re.M)}
    flash = sections.get(".text", 0) + sections.get(".data", 0)
    ram = sum(sections.get(name, 0) for name in (".data", ".bss", ".noinit"))
    if not (0 < flash < 61440 and 0 < ram < 3968):
        raise SystemExit(f"Memory budget failed: flash={flash}, SRAM={ram}")
    symbols = capture([nm, "-C", str(target.with_suffix(".elf"))])
    if variant["require_no_dynamic_allocation"] and re.search(
            r"\b[TwW] (malloc|calloc|realloc|_Zn\w+|operator new.*)$", symbols, re.M):
        raise SystemExit("Stack painting requires a controller with no dynamic allocation")
    for suffix, name in [
        (".bin", variant["bin_name"]), (".hex", "ambika_controller.hex"),
        (".elf", "ambika_controller.elf"),
    ]:
        shutil.copyfile(target.with_suffix(suffix), output / name)
    if (output / variant["bin_name"]).stat().st_size != flash:
        raise SystemExit("Unexpected BIN layout: size does not match flash accounting")
    shutil.copyfile(build / "controller.map", output / "controller.map")
    (output / "size.txt").write_text(section_text)
    (output / "symbols.txt").write_text(symbols)

# Keep the exact source inputs, including local avrlib and uncommitted diagnostics.
# Generated resource .cc/.h files are included; no regeneration occurs here.
source_files = [root / "makefile"]
for package in ("controller", "common", "avrlib"):
    for path in (root / package).rglob("*"):
        if path.is_file() and not any(part.startswith(".") or part == "hardware_design"
                                      for part in path.relative_to(root).parts):
            if path.suffix in (".h", ".cc", ".c", ".s", ".S", ".mk", ".py", ".inc") or path.name == "makefile":
                source_files.append(path)
source_files += [Path(__file__).resolve()]
with tarfile.open(output / "source.tar.gz", "w:gz") as archive:
    for path in sorted(source_files):
        archive.add(path, arcname=str(path.relative_to(root)))
shutil.copyfile(root / "common/features.h", output / "features.h")
version = re.search(r"kSystemVersion\s*=\s*0x([0-9a-fA-F]{2})",
                    (root / "controller/controller.h").read_text()).group(1)
manifest = {
    "label": variant["label"], "variant": args.variant,
    "displayed_version": f"v{version[0]}.{version[1]}",
    "git_head": capture(["git", "rev-parse", "HEAD"]).strip(),
    "source_state": "Working tree; exact source inputs included in source.tar.gz",
    "compiler": capture([gcc, "--version"]).splitlines()[0],
    "objcopy": capture([objcopy, "--version"]).splitlines()[0],
    "make_command": command,
    "flash_bytes": flash, "static_sram_bytes": ram, "remaining_sram_bytes": 4096 - ram,
    "source_sha256": {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
                      for p in sorted(source_files)},
    "artifact_sha256": {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                        for p in sorted(output.iterdir()) if p.is_file()},
}
(output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
print(f"Built {output / variant['bin_name']} ({args.variant}, displays v{version[0]}.{version[1]})")
print(f"Flash {flash}/61440; static SRAM {ram}/3968; runtime headroom {4096 - ram}")
