#!/usr/bin/env python3
"""Run production UI methods with checked host substitutes under ASan/UBSan."""
from pathlib import Path
import os
import argparse
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--sanitizers", default="address,undefined",
                    help="Compiler sanitizers (default: address,undefined); 'none' disables")
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix="kz-ui-tests-") as directory:
    temp = Path(directory)
    # Only peripheral/type headers are substituted; both .cc files are compiled
    # directly from the working tree by controller_ui_test.cc.
    headers = [
        "controller/ui_pages/parameter_editor.h", "avrlib/string.h",
        "controller/display.h", "controller/leds.h", "controller/multi.h",
        "controller/parameter.h", "controller/system_settings.h",
        "controller/ui_pages/os_info_page.h", "avr/eeprom.h",
        "avrlib/watchdog_timer.h", "controller/diagnostics.h", "controller/storage.h",
        "avrlib/base.h", "controller/midi_dispatcher.h",
        "controller/ui_pages/voice_assigner.h",
    ]
    for name in headers:
        path = temp / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("// Host substitute declarations are in controller_ui_test.cc.\n")
    binary = temp / "controller-ui-test"
    sanitizer_flags = [] if args.sanitizers == "none" else [f"-fsanitize={args.sanitizers}"]
    subprocess.run([
        os.environ.get("CXX", "clang++"), "-std=c++17", "-g", "-O1",
        *sanitizer_flags, "-fno-omit-frame-pointer",
        "-I", str(temp), "-I", str(root),
        str(root / "tests/controller_ui_test.cc"), "-o", str(binary),
    ], check=True)
    subprocess.run([str(binary)], check=True)
