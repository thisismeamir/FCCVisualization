// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
/**
 * @file GoldenText.h
 * @brief Text golden files (SVG, CSV, help output) in tests/data/golden.
 *
 * Same workflow as GoldenImage.h: `FCCVIS_UPDATE_GOLDEN=1` rewrites the
 * golden; on a mismatch the actual text is written to
 * `<build>/tests/golden-out/<name>` and the first differing line is reported.
 */
#pragma once

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fccvis::test {

/** @brief Outcome of a text golden comparison. */
struct GoldenTextResult {
  bool ok = false;      ///< Equal (or updated).
  std::string message;  ///< Explanation.
};

/** @brief Compares @p text with `tests/data/golden/<name>` (name includes the extension). */
inline GoldenTextResult CompareGoldenText(const std::string& text, const std::string& name) {
  namespace fs = std::filesystem;
  const fs::path golden = fs::path(FCCVIS_TEST_SOURCE_DIR) / "data" / "golden" / name;
  GoldenTextResult r;
  if (const char* u = std::getenv("FCCVIS_UPDATE_GOLDEN"); u && *u && std::string(u) != "0") {
    fs::create_directories(golden.parent_path());
    std::ofstream(golden, std::ios::binary) << text;
    r.ok = true;
    r.message = "updated golden " + golden.string();
    return r;
  }
  std::ifstream in(golden, std::ios::binary);
  std::stringstream ss;
  ss << in.rdbuf();
  const std::string expected = ss.str();
  if (in && expected == text) {
    r.ok = true;
    r.message = "matches " + golden.string();
    return r;
  }
  const fs::path outDir = fs::path(FCCVIS_TEST_BINARY_DIR) / "golden-out";
  fs::create_directories(outDir);
  std::ofstream(outDir / name, std::ios::binary) << text;
  std::istringstream a(text), b(expected);
  std::string la, lb;
  int line = 0;
  while (true) {
    ++line;
    const bool ga = static_cast<bool>(std::getline(a, la));
    const bool gb = static_cast<bool>(std::getline(b, lb));
    if (!ga && !gb) break;
    if (!ga || !gb || la != lb) break;
  }
  r.message = (in ? "differs from " : "missing golden ") + golden.string() + " (first difference at line " +
              std::to_string(line) + "); actual: " + (outDir / name).string() +
              "; rerun with FCCVIS_UPDATE_GOLDEN=1 if the change is intended";
  return r;
}

}  // namespace fccvis::test
