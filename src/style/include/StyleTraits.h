/**
 * @file StyleTraits.h
 * @brief Mapping from element type to style record (`StyleTraits<X>`, `StyleOf<X>`).
 *
 * The primary template and the specializations for the built-in element
 * kinds (`Element<Point>` → PointStyle, `Element<Line>` → LineStyle,
 * `Element<Surface>` → SurfaceStyle) are defined in Style.h, next to the
 * style records they name. This header exists so code can include the traits
 * by their own name. A new element kind adds one specialization:
 *
 * @code
 * template <> struct fccvis::style::StyleTraits<data::Element<MyKind>> { using type = MyStyle; };
 * @endcode
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include "Style.h"
