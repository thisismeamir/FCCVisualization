// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// TGeo tessellation, moved out of core geometry (INVENTORY item 3).
#include "TGeoShapeAdapter.h"

#include <TBuffer3D.h>
#include <TGeoCompositeShape.h>
#include <TGeoShapeAssembly.h>

#include <algorithm>
#include <cmath>
#include <mutex>
#include <cstdint>
#include <vector>

namespace fccvis::geometry {

namespace {

std::vector<std::uint32_t> PolygonLoop(const TBuffer3D& b, const Int_t* pol) {
  const int nSegs = pol[1];
  const Int_t* segIds = pol + 2;

  auto seg = [&](int i) { return b.fSegs + 3 * segIds[i]; };

  std::vector<std::uint32_t> loop;
  loop.reserve(nSegs);

  const std::uint32_t a = seg(0)[1];
  const std::uint32_t c = seg(0)[2];
  loop.push_back(a);
  std::uint32_t next = c;

  for (int i = 1; i < nSegs; ++i) {
    loop.push_back(next);
    const Int_t* s = seg(i);
    // Segment i shares `next` with the previous one; advance to its other end.
    next = (static_cast<std::uint32_t>(s[1]) == next) ? s[2] : s[1];
  }
  return loop;
}

int SegmentsFromTolerance(double relTolerance) {
  // sagitta/R = 1 - cos(pi/n)  =>  n = pi / acos(1 - relTolerance)
  const double t = std::clamp(relTolerance, 1e-4, 0.2);
  return std::clamp(static_cast<int>(std::ceil(M_PI / std::acos(1.0 - t))), 8, 360);
}

}  // namespace

Mesh TGeoShapeAdapter::Tessellate(double tolerance) const {
  // GetBuffer3D fills buffers shared by all shapes of a class and reads the
  // global segment count: serialise (backends tessellate on their own threads).
  static std::mutex lock;
  std::lock_guard<std::mutex> serialise(lock);
  // Assemblies and composite (boolean) shapes have no direct surface
  // tessellation here; the detector adapter reports them as skipped.
  if (dynamic_cast<const TGeoShapeAssembly*>(m_shape.get()) ||
      dynamic_cast<const TGeoCompositeShape*>(m_shape.get()))
    return Mesh{};

  SegmentsGuard guard(SegmentsFromTolerance(tolerance));

  const TBuffer3D& b = m_shape->GetBuffer3D(
      TBuffer3D::kCore | TBuffer3D::kBoundingBox | TBuffer3D::kShapeSpecific |
          TBuffer3D::kRawSizes | TBuffer3D::kRaw,
      /*localFrame=*/kTRUE);

  Mesh mesh;
  if (b.SectionsValid(TBuffer3D::kRaw) && b.NbPnts() > 0 && b.NbPols() > 0) {
    mesh.vertices.reserve(b.NbPnts());
    for (UInt_t i = 0; i < b.NbPnts(); ++i)
      mesh.vertices.push_back(
          {static_cast<float>(b.fPnts[3 * i] * kMillimetresPerTGeoUnit),
           static_cast<float>(b.fPnts[3 * i + 1] * kMillimetresPerTGeoUnit),
           static_cast<float>(b.fPnts[3 * i + 2] * kMillimetresPerTGeoUnit)});
    const Int_t* pol = b.fPols;
    for (UInt_t p = 0; p < b.NbPols(); ++p) {
      const auto loop = PolygonLoop(b, pol);
      for (std::size_t k = 1; k + 1 < loop.size(); ++k)
        mesh.triangles.push_back({loop[0], loop[k], loop[k + 1]});
      pol += 2 + pol[1];
    }
  }
  return mesh;
}

TGeoHMatrix ToTGeoHMatrix(const Transform& transform) {
  TGeoHMatrix matrix;
  const auto& t = transform.Translation();
  const auto& r = transform.Rotation();
  Double_t translation[3] = {t[0] / kMillimetresPerTGeoUnit, t[1] / kMillimetresPerTGeoUnit,
                             t[2] / kMillimetresPerTGeoUnit};
  Double_t rotation[9] = {r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8]};
  matrix.SetTranslation(translation);
  matrix.SetRotation(rotation);
  return matrix;
}

Transform ToTransform(const TGeoMatrix& m) {
  const Double_t* r = m.GetRotationMatrix();  // 9 values, row-major
  const Double_t* t = m.GetTranslation();     // 3 values
  Transform::Matrix R;
  for (int i = 0; i < 9; ++i) R[i] = r[i];
  Transform::Vector T = {t[0] * kMillimetresPerTGeoUnit, t[1] * kMillimetresPerTGeoUnit,
                         t[2] * kMillimetresPerTGeoUnit};
  return Transform(T, R);
}

}  // namespace fccvis::geometry
