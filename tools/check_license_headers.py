#!/usr/bin/env python3
"""Checks (or, with --fix, adds) the licence notice of fccvis' own source files.

Public headers (*.h under src/) need the Apache-2.0 notice and the @author line
of CONVENTIONS.md in their first Doxygen block; other C++, Python and
JavaScript sources need an SPDX line. Vendored code (third_party) and data
files are not checked. Run by CTest as build::license_headers.
"""
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
AUTHOR = " * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>\n"
COPYRIGHT = (" * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache\n"
             " *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0\n")
SPDX = {"//": "// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0\n",
        "#": "# Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0\n"}


def files():
    out = subprocess.run(["git", "ls-files"], cwd=ROOT, capture_output=True, text=True, check=True).stdout.split()
    for f in out:
        if f.startswith(("third_party/", "tests/data/")) or "node_modules" in f:
            continue
        if f.endswith((".h", ".hxx", ".cc", ".cpp", ".py", ".mjs", ".js")):
            yield f


def check(path, fix):
    text = (ROOT / path).read_text()
    if path.startswith("src/") and path.endswith((".h", ".hxx")):
        if "Apache" in text and "@author" in text:
            return True
        if not fix:
            return False
        m = re.search(r"/\*\*.*?@file.*?\n(\s*\*/)", text, re.S)
        if not m:
            return False
        text = text[:m.start(1)] + " *\n" + AUTHOR + COPYRIGHT + text[m.start(1):]
    else:
        if "Apache" in text:
            return True
        if not fix:
            return False
        marker = "#" if path.endswith(".py") else "//"
        if text.startswith("#!"):
            first, rest = text.split("\n", 1)
            text = first + "\n" + SPDX[marker] + rest
        else:
            text = SPDX[marker] + text
    (ROOT / path).write_text(text)
    return True


def main():
    fix = "--fix" in sys.argv
    bad = [f for f in files() if not check(f, fix)]
    for f in bad:
        print("missing licence notice:", f)
    print(f"checked licence notices: {len(bad)} missing")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
