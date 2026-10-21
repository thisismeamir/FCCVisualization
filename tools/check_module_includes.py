#!/usr/bin/env python3
# Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
"""Checks that every #include respects the fccvis module graph.

Reads the graph that CMake writes (fccvis_module_graph.json). Each source or
header belongs to the module whose directory is the longest prefix of its
path. A quoted include must resolve to a header of the same module or of a
module whose kind is allowed for the including module. Core modules (any kind
that may not link third-party targets) must not include ROOT, podio, EDM4hep
or DD4hep headers.

Exit code 0 when clean, 1 with one line per violation otherwise.
"""
import json
import os
import re
import sys

THIRD_PARTY_KINDS = {"adapter", "backend_impl", "cli"}
FOREIGN = re.compile(r'^(T[A-Z]\w*\.h|R[A-Z]\w*\.h|Rtypes\.h|ROOT/|podio/|edm4hep/|DD4hep/|Gui|Eve)')
INCLUDE = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]')
SUFFIXES = (".h", ".hpp", ".cc", ".cpp", ".cxx")


def main(graph_path: str) -> int:
    with open(graph_path, encoding="utf-8") as f:
        modules = json.load(f)["modules"]

    dirs = sorted(((os.path.normpath(m["dir"]), name) for name, m in modules.items()),
                  key=lambda t: -len(t[0]))

    def owner(path: str):
        path = os.path.normpath(path)
        for d, name in dirs:
            if path == d or path.startswith(d + os.sep):
                return name
        return None

    files = {}
    headers = {}
    module_dirs = {d for d, _ in dirs}
    for d, _ in dirs:
        for root, subdirs, names in os.walk(d):
            # A subdirectory with its own CMakeLists.txt is another module (or
            # a group of them); it is checked on its own when it is declared,
            # and skipped when it is disabled.
            subdirs[:] = [s for s in subdirs
                          if os.path.normpath(os.path.join(root, s)) not in module_dirs
                          and not os.path.exists(os.path.join(root, s, "CMakeLists.txt"))]
            for n in names:
                if not n.endswith(SUFFIXES):
                    continue
                p = os.path.join(root, n)
                m = owner(p)
                files[p] = m
                if n.endswith((".h", ".hpp")):
                    headers.setdefault(n, set()).add(m)

    errors = []
    for path, mod in sorted(files.items()):
        kind = modules[mod]["kind"]
        allowed = {name for name, m in modules.items() if m["kind"] in modules[mod]["allowed_kinds"]}
        allowed.add(mod)
        with open(path, encoding="utf-8", errors="replace") as f:
            for lineno, line in enumerate(f, 1):
                hit = INCLUDE.match(line)
                if not hit:
                    continue
                target = hit.group(2)
                base = os.path.basename(target)
                if kind not in THIRD_PARTY_KINDS and FOREIGN.match(target):
                    errors.append(f"{path}:{lineno}: core module '{mod}' includes third-party header <{target}>")
                    continue
                if hit.group(1) != '"':
                    continue
                owners = headers.get(base)
                if not owners:
                    continue  # generated or third-party header
                if not owners & allowed:
                    errors.append(f"{path}:{lineno}: module '{mod}' (kind {kind}) includes "
                                  f"'{target}' from {sorted(owners)}, not allowed by the module graph")

    for e in errors:
        print(e)
    print(f"checked {len(files)} files in {len(modules)} modules: {len(errors)} violation(s)")
    return 1 if errors else 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("usage: check_module_includes.py <fccvis_module_graph.json>")
        sys.exit(2)
    sys.exit(main(sys.argv[1]))
