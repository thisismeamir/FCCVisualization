// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
/**
 * @file GoldenImage.h
 * @brief Visual regression harness: compare a rendered image with a golden PNG.
 *
 * Goldens live in `tests/data/golden/<name>.png`. To create or refresh them,
 * run the tests with `FCCVIS_UPDATE_GOLDEN=1`; review the new files before
 * committing them. On a mismatch the actual image and a diff image (changed
 * pixels in magenta over a faded copy of the golden) are written to
 * `<build>/tests/golden-out/` and their paths are reported.
 */
#pragma once

#include "Png.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace fccvis::test {

/** @brief Outcome of a golden comparison. */
struct GoldenResult {
  bool ok = false;        ///< Image matches (or golden was updated).
  double fraction = 1.0;  ///< Fraction of differing pixels.
  std::string message;    ///< Human-readable explanation.
};

/**
 * @brief Compares @p image with the golden @p name.
 * @param maxFraction      Largest fraction of differing pixels accepted.
 * @param channelTolerance Largest per-channel difference counted as equal.
 */
inline GoldenResult CompareGolden(const backend::raster::Image& image, const std::string& name,
                                  double maxFraction = 0.002, int channelTolerance = 8) {
  namespace fs = std::filesystem;
  namespace rs = backend::raster;
  const fs::path golden = fs::path(FCCVIS_TEST_SOURCE_DIR) / "data" / "golden" / (name + ".png");
  const fs::path outDir = fs::path(FCCVIS_TEST_BINARY_DIR) / "golden-out";
  GoldenResult r;

  if (const char* u = std::getenv("FCCVIS_UPDATE_GOLDEN"); u && *u && std::string(u) != "0") {
    fs::create_directories(golden.parent_path());
    r.ok = rs::WritePng(golden.string(), image);
    r.fraction = 0;
    r.message = (r.ok ? "updated golden " : "cannot write golden ") + golden.string();
    return r;
  }

  fs::create_directories(outDir);
  const fs::path actual = outDir / (name + ".png");
  std::string error;
  const auto expected = rs::ReadPng(golden.string(), &error);
  if (!expected) {
    rs::WritePng(actual.string(), image);
    r.message = "golden " + golden.string() + " unreadable (" + error + "); actual image written to " +
                actual.string() + "; rerun with FCCVIS_UPDATE_GOLDEN=1 to accept it";
    return r;
  }

  r.fraction = rs::DifferentPixelFraction(image, *expected, channelTolerance);
  r.ok = r.fraction <= maxFraction;
  if (r.ok) {
    r.message = "matches " + golden.string();
    return r;
  }

  rs::WritePng(actual.string(), image);
  const fs::path diffPath = outDir / (name + ".diff.png");
  if (expected->width == image.width && expected->height == image.height) {
    rs::Image diff = *expected;
    for (std::size_t i = 0; i + 3 < diff.rgba.size(); i += 4) {
      int worst = 0;
      for (int c = 0; c < 4; ++c)
        worst = std::max(worst, std::abs(int(image.rgba[i + c]) - int(expected->rgba[i + c])));
      if (worst > channelTolerance) {
        diff.rgba[i] = 255; diff.rgba[i + 1] = 0; diff.rgba[i + 2] = 255; diff.rgba[i + 3] = 255;
      } else {
        for (int c = 0; c < 3; ++c) diff.rgba[i + c] = static_cast<std::uint8_t>(191 + diff.rgba[i + c] / 4);
      }
    }
    rs::WritePng(diffPath.string(), diff);
  }
  r.message = "differs from " + golden.string() + " in " + std::to_string(r.fraction * 100) +
              "% of pixels (limit " + std::to_string(maxFraction * 100) + "%); actual: " + actual.string() +
              ", diff: " + diffPath.string() + "; rerun with FCCVIS_UPDATE_GOLDEN=1 if the change is intended";
  return r;
}

}  // namespace fccvis::test
