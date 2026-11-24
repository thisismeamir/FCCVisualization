/**
 * @file CedProtocol.h
 * @brief Wire format of the CED event display (iLCSoft/CED, `glced`), as
 *        spoken by the CED backend.
 *
 * A client sends, per element type, one message `[int size][unsigned type]`
 * followed by `count` items of that type (`size` includes the 8-byte
 * header), then a `DRAW_EVENT` message (header only) that makes the server
 * show the new event. Element type numbers are the registration order of
 * CED's `ced_register_elements()` (identical in client and server). Values
 * are in the host's byte order, as in CED itself. When the user picks an
 * object in glced (double click), the server sends back the object's id as
 * one `int`.
 *
 * Only the element types the backend uses are described here. The layout
 * follows CED 1.10 (`ced_cli.h`); it is an independent description of the
 * protocol for interoperability: no CED code is used or linked.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace fccvis::backend::ced {

/** @brief Element type numbers (CED registration order). */
enum Type : std::uint32_t {
  kLine = 2,          ///< CED_Line
  kHit = 3,           ///< CED_Hit
  kLayerText = 10,    ///< CED_TEXT: description of a visibility layer
  kDrawEvent = 10000  ///< show the event sent so far
};

/** @brief Hit shapes (CED_HIT_TYPE). */
enum HitShape : std::uint32_t { kPoint = 0, kCross = 1, kStar = 2, kBox = 3 };

/** @brief A point (CED_Hit, 36 bytes). */
struct Hit {
  float x, y, z;            ///< Position (mm).
  float time = 0;           ///< For animation; 0.
  std::uint32_t shape = 0;  ///< HitShape.
  std::uint32_t layer = 0;  ///< Visibility layer (0..99).
  std::uint32_t color = 0;  ///< 0xTTRRGGBB, TT = transparency (255 - alpha).
  std::uint32_t size = 0;   ///< Size in pixels.
  std::uint32_t id = 0;     ///< Object id, returned when picked (0 = none).
};

/** @brief A line segment (CED_Line, 40 bytes). */
struct Line {
  float x0, y0, z0, x1, y1, z1;  ///< End points (mm).
  std::uint32_t layer = 0;       ///< Visibility layer.
  std::uint32_t width = 1;       ///< Width in pixels.
  std::uint32_t color = 0;       ///< 0xTTRRGGBB.
  std::uint32_t id = 0;          ///< Object id.
};

/** @brief Description of a visibility layer (CED_TEXT, 404 bytes). */
struct LayerText {
  char text[400] = {};  ///< Description shown in glced's layer menu.
  std::int32_t layer = 0;  ///< Layer number.
};

static_assert(sizeof(Hit) == 36 && sizeof(Line) == 40 && sizeof(LayerText) == 404, "CED item layout");

/** @brief Appends one message (header + items) to @p out. */
template <typename T>
void AppendMessage(std::vector<char>& out, Type type, const std::vector<T>& items) {
  if (items.empty()) return;
  const std::int32_t size = static_cast<std::int32_t>(8 + items.size() * sizeof(T));
  const std::uint32_t t = type;
  const auto at = out.size();
  out.resize(at + static_cast<std::size_t>(size));
  std::memcpy(out.data() + at, &size, 4);
  std::memcpy(out.data() + at + 4, &t, 4);
  std::memcpy(out.data() + at + 8, items.data(), items.size() * sizeof(T));
}

/** @brief Appends the DRAW_EVENT message. */
inline void AppendDraw(std::vector<char>& out) {
  const std::int32_t size = 8;
  const std::uint32_t t = kDrawEvent;
  const auto at = out.size();
  out.resize(at + 8);
  std::memcpy(out.data() + at, &size, 4);
  std::memcpy(out.data() + at + 4, &t, 4);
}

}  // namespace fccvis::backend::ced
