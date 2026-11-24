#!/usr/bin/env bash
# Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
#
# Release build matrix (DOCS_AND_RELEASE.md): configures, builds and tests
# fccvis in several configurations and writes one summary line per
# configuration to <out>/matrix.txt.
#   tools/release_matrix.sh <out-dir> [config ...]
# Configurations: none (every option off), core (default options, no
# third-party package searched), one per option (that option alone), all
# (everything found, docs on), static (static library only).
# Environment: CMAKE_PREFIX_PATH for ROOT/DD4hep/podio/EDM4hep, LD_LIBRARY_PATH
# for running their tests, JOBS (default: nproc).
set -u
SRC="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$(mkdir -p "$1" && cd "$1" && pwd)"; shift
JOBS="${JOBS:-$(nproc)}"
OPTIONS=(CSV PHOENIX_JSON DD4HEP EDM4HEP RASTER SVG HISTOGRAM CED PHOENIX ROOT REVE PYTHON)
NOTHIRD="-DCMAKE_DISABLE_FIND_PACKAGE_ROOT=ON -DCMAKE_DISABLE_FIND_PACKAGE_DD4hep=ON -DCMAKE_DISABLE_FIND_PACKAGE_EDM4HEP=ON -DCMAKE_DISABLE_FIND_PACKAGE_pybind11=ON"

alloff() { for o in "${OPTIONS[@]}"; do printf -- "-DFCCVIS_ENABLE_%s=OFF " "$o"; done; }

args_for() {
  case "$1" in
    none) echo "$(alloff) $NOTHIRD" ;;
    core) echo "$NOTHIRD" ;;
    all) echo "-DFCCVIS_BUILD_DOCS=ON" ;;
    static) echo "-DFCCVIS_BUILD_SHARED=OFF -DFCCVIS_BUILD_STATIC=ON" ;;
    only-REVE) echo "$(alloff) -DFCCVIS_ENABLE_ROOT=ON -DFCCVIS_ENABLE_REVE=ON" ;;
    only-*) echo "$(alloff) -DFCCVIS_ENABLE_${1#only-}=ON" ;;
  esac
}

CONFIGS=("$@")
if [ ${#CONFIGS[@]} -eq 0 ]; then
  CONFIGS=(none core)
  for o in "${OPTIONS[@]}"; do CONFIGS+=("only-$o"); done
  CONFIGS+=(all static)
fi

for c in "${CONFIGS[@]}"; do
  dir="$OUT/$c"
  rm -rf "$dir"; mkdir -p "$dir"
  start=$(date +%s)
  # shellcheck disable=SC2046
  if ! cmake -S "$SRC" -B "$dir" -G Ninja -DCMAKE_BUILD_TYPE=Release $(args_for "$c") > "$dir/configure.log" 2>&1; then
    echo "$c: configure FAILED (see $dir/configure.log)" >> "$OUT/matrix.txt"; continue
  fi
  if ! cmake --build "$dir" -j "$JOBS" > "$dir/build.log" 2>&1; then
    echo "$c: build FAILED (see $dir/build.log)" >> "$OUT/matrix.txt"; continue
  fi
  if grep -q "FCCVIS_BUILD_DOCS:BOOL=ON" "$dir/CMakeCache.txt"; then
    cmake --build "$dir" --target docs > "$dir/docs.log" 2>&1 && docs="docs ok" || docs="docs FAILED"
  else
    docs=""
  fi
  (cd "$dir" && ctest -j "$JOBS" --timeout 300 > ctest.log 2>&1)
  enabled=$(grep -E "^FCCVIS_ENABLE_[A-Z0-9_]+:BOOL=ON" "$dir/CMakeCache.txt" | sed 's/FCCVIS_ENABLE_//; s/:BOOL=ON//' | tr '\n' ' ')
  summary=$(grep -E "tests passed|No tests were found" "$dir/ctest.log" | tail -1)
  skipped=$(grep -c "(Skipped)" "$dir/ctest.log")
  failed=$(grep -E "\(Failed\)|\(Timeout\)|\(SEGFAULT\)|Not Run" "$dir/ctest.log" | sed 's/^\s*/    /')
  echo "$c: enabled [ ${enabled}] ${summary}; ${skipped} skipped${docs:+; $docs}; $(( $(date +%s) - start )) s" >> "$OUT/matrix.txt"
  [ -n "$failed" ] && echo "$failed" >> "$OUT/matrix.txt"
done
echo "matrix done" >> "$OUT/matrix.txt"
