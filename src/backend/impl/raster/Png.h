/**
 * @file Png.h
 * @brief Minimal PNG writer/reader (8-bit RGBA, no external dependency).
 *
 * The writer compresses with LZ77 and the fixed Huffman code of deflate
 * (no zlib needed). The reader decodes any non-interlaced 8-bit RGBA PNG
 * (stored, fixed and dynamic deflate blocks; all five row filters); it is what
 * the visual regression tests use to load goldens.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace fccvis::backend::raster {

/** @brief 8-bit RGBA image, rows top to bottom. */
struct Image {
  int width = 0;                    ///< Width in pixels.
  int height = 0;                   ///< Height in pixels.
  std::vector<std::uint8_t> rgba;   ///< width * height * 4 bytes.

  /** @brief Pixel (x, y) as 0xRRGGBBAA. */
  [[nodiscard]] std::uint32_t At(int x, int y) const;
};

/** @brief Encodes an image as PNG bytes. */
[[nodiscard]] std::vector<std::uint8_t> EncodePng(const Image& image);

/**
 * @brief Writes an image to a PNG file.
 * @return False if the file cannot be written.
 */
bool WritePng(const std::string& path, const Image& image);

/**
 * @brief Reads a non-interlaced 8-bit RGBA PNG (for example one written by WritePng).
 * @param error Receives the reason on failure.
 * @return The image, or nullopt for unreadable or unsupported files.
 */
[[nodiscard]] std::optional<Image> ReadPng(const std::string& path, std::string* error = nullptr);

/**
 * @brief Compares two images.
 * @param channelTolerance Largest per-channel difference counted as equal.
 * @return Fraction of pixels that differ by more than @p channelTolerance
 *         (1.0 if the sizes differ).
 */
[[nodiscard]] double DifferentPixelFraction(const Image& a, const Image& b, int channelTolerance);

}  // namespace fccvis::backend::raster
