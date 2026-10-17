/**
 * @file ShapeFilters.h
 * @brief Context filters: select elements by position relative to a shape.
 *
 * The shape is a placed geometry::Shape (shape plus transform to world
 * coordinates) or a bare AbstractShape used in world coordinates. Which
 * point(s) of an element are tested:
 * - Point: its position;
 * - Line: its vertices, combined by LineTest (any / all / centroid);
 * - Surface: the centre of its shape's bounding box, in world coordinates.
 *
 * `Within(d, shape)` uses the shape's criterion as a distance estimate:
 * it passes points inside the shape or at most @p d millimetres outside it.
 * Elements with no vertices never pass.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include "fccvis/stdlib/Filters.h"
#include "fccvis/stdlib/Shapes.h"

#include <memory>

namespace fccvis::stdlib {

/** @brief How a polyline is tested against a shape. */
enum class LineTest {
  AnyVertex,    ///< At least one vertex passes (default).
  AllVertices,  ///< Every vertex passes.
  Centroid      ///< The mean of the vertices passes.
};

/** @brief Elements inside @p shape (criterion <= 0). */
[[nodiscard]] Selector Inside(const geometry::Shape& shape, LineTest lines = LineTest::AnyVertex);
/** @brief Elements inside a shape given in world coordinates. */
[[nodiscard]] Selector Inside(ShapePtr shape, LineTest lines = LineTest::AnyVertex);

/** @brief Elements outside @p shape (criterion > 0); for lines, LineTest applies to "outside". */
[[nodiscard]] Selector Outside(const geometry::Shape& shape, LineTest lines = LineTest::AnyVertex);
/** @brief Elements outside a shape given in world coordinates. */
[[nodiscard]] Selector Outside(ShapePtr shape, LineTest lines = LineTest::AnyVertex);

/** @brief Elements inside @p shape or within @p distance millimetres of it. */
[[nodiscard]] Selector Within(double distance, const geometry::Shape& shape,
                              LineTest lines = LineTest::AnyVertex);
/** @brief Elements within @p distance of a shape given in world coordinates. */
[[nodiscard]] Selector Within(double distance, ShapePtr shape, LineTest lines = LineTest::AnyVertex);

/**
 * @brief World-space point used for a surface element: the centre of its
 *        shape's local bounding box, transformed by the surface's transform.
 */
[[nodiscard]] Vec3 SurfaceCentre(const data::Surface& surface);

}  // namespace fccvis::stdlib
