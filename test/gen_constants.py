#!/usr/bin/env python3
#
# gen_constants.py – Extract selected #define values from C source files for Python use.
#
# Parses C header/source files to extract important configuration constants
# (like timeouts, protocol version, and limits), and emits a Python module
# for use in test automation.
#
# Intended to ensure consistency between C implementation and test expectations.
#
#     Copyright 2025 Julian Friedrich
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is provided on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Source repository: https://github.com/fjulian79/wd-broker

import re
import ast
from pathlib import Path

# Konstanten, die übernommen werden sollen
WANTED_KEYS = {
    "PACKAGE_VERSION",
    "SOCKET_PROT_VERSION",
    "WD_MAX_CLIENTS",
    "WD_CLIENT_TIMEOUT_MIN_MS",
    "WD_CLIENT_TIMEOUT_MAX_MS",
}

# Regex für einfache #define-Zeilen
define_pattern = re.compile(r'#define\s+(\w+)\s+(.+)')

# Verzeichnisse, die durchsucht werden sollen
INPUT_DIRS = [Path("."), Path("src")]
OUTPUT_FILE = Path("test/constants.py")
constants = {}

def parse_value(val):
    val = val.strip()

    # Entferne äußere Klammern wie (1 * 100)
    if val.startswith("(") and val.endswith(")"):
        val = val[1:-1].strip()

    # String? Versuche sichere Auswertung
    if val.startswith('"') or val.startswith("'"):
        try:
            return ast.literal_eval(val)
        except Exception:
            return val.strip('"').strip("'")

    # Versuche, numerischen Ausdruck auszuwerten
    try:
        return ast.literal_eval(val)
    except Exception:
        try:
            return eval(val, {"__builtins__": {}})
        except Exception:
            return None  # Nicht interpretierbar

# C- und Header-Files durchsuchen
for input_dir in INPUT_DIRS:
    for filepath in input_dir.glob("**/*.[ch]"):
        with filepath.open() as f:
            for line in f:
                match = define_pattern.match(line.strip())
                if match:
                    key, val = match.groups()
                    if key in WANTED_KEYS:
                        parsed = parse_value(val)
                        if parsed is not None:
                            constants[key] = parsed

# Ausgabedatei erzeugen
OUTPUT_FILE.parent.mkdir(parents=True, exist_ok=True)
with OUTPUT_FILE.open("w") as f:
    f.write("# Auto-generated from C headers and source files\n\n")
    for key, value in sorted(constants.items()):
        if isinstance(value, str):
            f.write(f"{key} = {value!r}\n")  # sauber gequoted
        else:
            f.write(f"{key} = {value}\n")
