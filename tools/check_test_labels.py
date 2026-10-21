#!/usr/bin/env python3
# Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
"""Checks that every registered CTest test carries a dependency label.

Usage: check_test_labels.py <ctest executable> <build dir>

Each test must have `core`, `build`, or at least one `needs-*` label
(docs/DEPENDENCIES.md). Prints the offending tests and exits 1 otherwise.
"""
import json
import subprocess
import sys


def main(ctest: str, build_dir: str) -> int:
    out = subprocess.run([ctest, "--show-only=json-v1"], cwd=build_dir,
                         capture_output=True, text=True, check=True).stdout
    tests = json.loads(out).get("tests", [])
    bad = []
    for t in tests:
        labels = []
        for p in t.get("properties", []):
            if p.get("name") == "LABELS":
                labels = p.get("value", [])
        ok = ("core" in labels or "build" in labels
              or any(l.startswith("needs-") for l in labels))
        if not ok:
            bad.append(t["name"])
    for name in bad:
        print(f"unlabelled test: {name}")
    print(f"checked {len(tests)} tests: {len(bad)} without a dependency label")
    return 1 if bad else 0


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(2)
    sys.exit(main(sys.argv[1], sys.argv[2]))
