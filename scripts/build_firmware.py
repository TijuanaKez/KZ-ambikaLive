#!/usr/bin/env python3
"""Build/package a controller or voice card image, enforcing its memory limits.

Targets:
  controller   ATmega644P motherboard. Variants:
                 release     Shipping firmware, keeps the SD firmware-update
                             page. BIN is AMBIKA.BIN, ready for a card root.
                 diagnostic  DIAG3 memory-instrumented image. Boots into the
                             RAM/LOW screen, replaces the firmware-update page.
  voicecard    ATmega328P voice card. BIN is VOICE.BIN; rename per card number.

WARNING on voicecard images: no freshly compiled voice card firmware has ever
been validated on hardware in this project, and Carey's historical notes record
that later compilers could build the voice card while producing a SILENT card.
Treat any voicecard output here as a test build until proven otherwise.
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

# Flash limits are the device size less its bootloader. SRAM limits reserve a
# stack margin, per Pichenettes' original build guidance.
TARGETS = {
    "controller": {
        "makefile": "controller/makefile",
        "target_dir": "ambika_controller",
        "max_flash": 61440,
        "max_sram": 3968,
        "total_sram": 4096,
        "variants": {
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
        },
        "packages": ("controller", "common", "avrlib"),
    },
    "voicecard": {
        "makefile": "voicecard/makefile",
        "target_dir": "ambika_voicecard",
        "max_flash": 31744,
        "max_sram": 1920,
        "total_sram": 2048,
        "variants": {
            "release": {
                # The voicecard makefile supplies its own EXTRA_DEFINES; passing
                # an empty one here leaves them in place.
                "defines": "",
                "bin_name": "VOICE.BIN",
                "label": "KZ Ambika Live voice card",
                "require_no_dynamic_allocation": False,
            },
        },
        "packages": ("voicecard", "common", "avrlib"),
    },
}

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("output", type=Path, help="New directory for reviewed build artifacts")
parser.add_argument("--target", choices=sorted(TARGETS), default="controller")
parser.add_argument("--variant", default="release",
                    help="controller: release or diagnostic; voicecard: release")
args = parser.parse_args()
target = TARGETS[args.target]
if args.variant not in target["variants"]:
    raise SystemExit(f"{args.target} has no variant {args.variant!r}; "
                     f"choose from {sorted(target['variants'])}")
variant = target["variants"][args.variant]
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
with tempfile.TemporaryDirectory(prefix=f"kz-{args.target}-{args.variant}-") as directory:
    build = Path(directory)
    command = [
        tool("make"), "-f", target["makefile"], "bin", "all",
        f"BUILD_ROOT={build}/",
        f"AVRLIB_TOOLS_PATH={Path(gcc).parent}/",
        f"AVRLIB_TOOLS_PATH_LEGACY={Path(objcopy).parent}/",
        *([f"EXTRA_DEFINES={defines}"] if defines else []),
        f"EXTRA_LD_FLAGS=,-Map,{build}/firmware.map",
    ]
    with (output / "build.log").open("w") as log:
        subprocess.run(command, cwd=root, stdout=log, stderr=subprocess.STDOUT, check=True)
    stem = build / target["target_dir"] / target["target_dir"]
    section_text = capture([size, "-A", str(stem.with_suffix(".elf"))])
    sections = {name: int(count) for name, count in
                re.findall(r"^(\.\S+)\s+(\d+)\s+\d+", section_text, re.M)}
    flash = sections.get(".text", 0) + sections.get(".data", 0)
    ram = sum(sections.get(name, 0) for name in (".data", ".bss", ".noinit"))
    if not (0 < flash < target["max_flash"] and 0 < ram < target["max_sram"]):
        raise SystemExit(f"Memory budget failed: flash={flash}/{target['max_flash']}, "
                         f"SRAM={ram}/{target['max_sram']}")
    symbols = capture([nm, "-C", str(stem.with_suffix(".elf"))])
    if variant["require_no_dynamic_allocation"] and re.search(
            r"\b[TwW] (malloc|calloc|realloc|_Zn\w+|operator new.*)$", symbols, re.M):
        raise SystemExit("Stack painting requires a controller with no dynamic allocation")
    for suffix, name in [
        (".bin", variant["bin_name"]),
        (".hex", f"{target['target_dir']}.hex"),
        (".elf", f"{target['target_dir']}.elf"),
    ]:
        shutil.copyfile(stem.with_suffix(suffix), output / name)
    if (output / variant["bin_name"]).stat().st_size != flash:
        raise SystemExit("Unexpected BIN layout: size does not match flash accounting")
    shutil.copyfile(build / "firmware.map", output / f"{target['target_dir']}.map")
    (output / "size.txt").write_text(section_text)
    (output / "symbols.txt").write_text(symbols)

# Keep the exact source inputs, including local avrlib and uncommitted diagnostics.
# Generated resource .cc/.h files are included; no regeneration occurs here.
source_files = [root / "makefile"]
for package in target["packages"]:
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
version_header = "controller/controller.h" if args.target == "controller" else "voicecard/voicecard.h"
version = re.search(r"kSystemVersion\s*=\s*0x([0-9a-fA-F]{2})",
                    (root / version_header).read_text()).group(1)
manifest = {
    "label": variant["label"], "target": args.target, "variant": args.variant,
    "displayed_version": f"v{version[0]}.{version[1]}",
    "git_head": capture(["git", "rev-parse", "HEAD"]).strip(),
    "source_state": "Working tree; exact source inputs included in source.tar.gz",
    "compiler": capture([gcc, "--version"]).splitlines()[0],
    "objcopy": capture([objcopy, "--version"]).splitlines()[0],
    "make_command": command,
    "flash_bytes": flash, "max_flash_bytes": target["max_flash"],
    "static_sram_bytes": ram, "max_static_sram_bytes": target["max_sram"],
    "remaining_sram_bytes": target["total_sram"] - ram,
    "source_sha256": {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
                      for p in sorted(source_files)},
    "artifact_sha256": {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                        for p in sorted(output.iterdir()) if p.is_file()},
}
(output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
print(f"Built {output / variant['bin_name']} "
      f"({args.target}/{args.variant}, reports v{version[0]}.{version[1]})")
print(f"Flash {flash}/{target['max_flash']}; static SRAM {ram}/{target['max_sram']}; "
      f"runtime headroom {target['total_sram'] - ram}")
