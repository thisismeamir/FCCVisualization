#pragma once

/**
 * @file StyleTraits.h
 * @brief Maps each element kind to its style record.
 *
 * Kept apart from Style.h so that Style.h stays independent of the element
 * model. New element kinds specialize StyleTraits in their own headers.
 */

#include "Elements.h"
#include "Style.h"

namespace fccvis::style {

template <> struct StyleTraits<data::Element<data::Point>>   { using type = PointStyle; };
template <> struct StyleTraits<data::Element<data::Line>>    { using type = LineStyle; };
template <> struct StyleTraits<data::Element<data::Surface>> { using type = SurfaceStyle; };

}  // namespace fccvis::style
