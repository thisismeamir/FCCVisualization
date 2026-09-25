#pragma once

#include "Geometry.h"
#include <DD4hep/DetElement.h>
#include <DD4hep/Detector.h>
#include <TGeoShape.h>
namespace fccvis::geometry {

struct SegmentsGuard {
  int old;
  explicit SegmentsGuard(int n)
      : old(gGeoManager ? gGeoManager->GetNsegments() : 0) {
    if (gGeoManager) gGeoManager->SetNsegments(n);
  }
  ~SegmentsGuard() { if (gGeoManager) gGeoManager->SetNsegments(old); }
};

class TGeoShapeAdapter final : public AbstractShape {
public:
  explicit TGeoShapeAdapter(std::shared_ptr<const TGeoShape> shape)
      : m_shape(std::move(shape)) {}

  std::shared_ptr<AbstractShape> Clone() const override {
    // The TGeoShape is immutable and owned by the TGeoManager, so sharing is a
    // valid deep copy.
    return std::make_shared<TGeoShapeAdapter>(m_shape);
  }
  double Criterion(const Vector &p) const override {
    double x[3] = {p[0], p[1], p[2]};
    constexpr double kEps =
        1e-9; // keeps "outside" strictly > 0 if Safety() returns 0
    if (m_shape->Contains(x))
      return -m_shape->Safety(x, /*in=*/true);
    return std::max(m_shape->Safety(x, /*in=*/false), kEps);
  }
  Mesh
  Tessellate(double tolerance) const override; // the TBuffer3D implementation

private:
  std::shared_ptr<const TGeoShape> m_shape;
};

} // namespace fccvis::geometry
