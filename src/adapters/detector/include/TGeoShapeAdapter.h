/**
 * @file TGeoShapeAdapter.h
 * @brief Wraps a ROOT TGeoShape as a fccvis::geometry::AbstractShape.
 *
 * Part of the detector adapter (A1). This header uses ROOT types and is only
 * available when the build enables `FCCVIS_ENABLE_DD4HEP`; it must never be
 * included from core modules.
 *
 * Units: fccvis lengths are millimetres; TGeo lengths are centimetres. The
 * adapter converts at its boundary (Criterion input and output, tessellated
 * vertices), so callers always work in millimetres.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include "Geometry.h"

#include <TGeoManager.h>
#include <TGeoMatrix.h>
#include <TGeoShape.h>

#include <algorithm>
#include <memory>

namespace fccvis::geometry {

/** @brief Millimetres per TGeo length unit (centimetre). */
inline constexpr double kMillimetresPerTGeoUnit = 10.0;

/**
 * @brief Temporarily sets the global TGeo segment count; restores it on exit.
 */
struct SegmentsGuard {
  int old;  ///< Segment count to restore.

  /** @param n Segment count to use while the guard lives. */
  explicit SegmentsGuard(int n)
      : old(gGeoManager ? gGeoManager->GetNsegments() : 0) {
    if (gGeoManager) gGeoManager->SetNsegments(n);
  }
  ~SegmentsGuard() {
    if (gGeoManager) gGeoManager->SetNsegments(old);
  }
  SegmentsGuard(const SegmentsGuard&) = delete;
  SegmentsGuard& operator=(const SegmentsGuard&) = delete;
};

/**
 * @brief AbstractShape backed by a TGeoShape.
 *
 * The TGeoShape is not owned in the usual case (the TGeoManager owns it); the
 * shared pointer typically carries a no-op deleter.
 */
class TGeoShapeAdapter final : public AbstractShape {
 public:
  /** @param shape Shape to wrap; must not be null. */
  explicit TGeoShapeAdapter(std::shared_ptr<const TGeoShape> shape)
      : m_shape(std::move(shape)) {}

  /** @brief Shares the immutable TGeoShape. */
  std::shared_ptr<AbstractShape> Clone() const override {
    return std::make_shared<TGeoShapeAdapter>(m_shape);
  }

  /**
   * @brief Signed distance estimate in millimetres: <= 0 inside, > 0 outside.
   * @param p Point in the shape's local frame, millimetres.
   */
  double Criterion(const Vector& p) const override {
    double x[3] = {p[0] / kMillimetresPerTGeoUnit, p[1] / kMillimetresPerTGeoUnit,
                   p[2] / kMillimetresPerTGeoUnit};
    constexpr double kEps = 1e-9;  // keeps "outside" strictly > 0 if Safety() returns 0
    if (m_shape->Contains(x))
      return -m_shape->Safety(x, /*in=*/true) * kMillimetresPerTGeoUnit;
    return std::max(m_shape->Safety(x, /*in=*/false) * kMillimetresPerTGeoUnit, kEps);
  }

  /**
   * @brief Tessellates through TGeoShape::GetBuffer3D.
   * @param tolerance Relative sagitta tolerance; mapped to a segment count.
   * @return Mesh in millimetres; empty for assemblies and composite shapes.
   */
  Mesh Tessellate(double tolerance) const override;

  /** @brief ROOT class name of the wrapped shape, for skip reports. */
  [[nodiscard]] const char* RootClassName() const { return m_shape->ClassName(); }

 private:
  std::shared_ptr<const TGeoShape> m_shape;
};

/** @brief Converts a fccvis Transform (millimetres) to a TGeoHMatrix (centimetres). */
TGeoHMatrix ToTGeoHMatrix(const Transform& transform);

/** @brief Converts a TGeoMatrix (centimetres) to a fccvis Transform (millimetres). */
Transform ToTransform(const TGeoMatrix& m);

}  // namespace fccvis::geometry
