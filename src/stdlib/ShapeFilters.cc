// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
#include "fccvis/stdlib/ShapeFilters.h"

#include <functional>
#include <map>
#include <mutex>
#include <stdexcept>

namespace fccvis::stdlib {

namespace {

Vec3 Centre(const Bounds& b) {
  if (b.Empty()) return {0, 0, 0};
  return {(b.lo[0] + b.hi[0]) / 2, (b.lo[1] + b.hi[1]) / 2, (b.lo[2] + b.hi[2]) / 2};
}

// Bounding-box centres of non-standard shapes are expensive (tessellation);
// each selector remembers them per shape object. Keys are weak references, so
// a freed shape can never be confused with a new one at the same address.
class CentreCache {
 public:
  Vec3 Get(const data::Surface& surface) {
    const auto g = surface.shape.GeometryPtr();
    Vec3 local{0, 0, 0};
    if (!g) return surface.shape.GetTransform().Apply(local);
    if (const auto* known = dynamic_cast<const StandardShape*>(g.get())) {
      local = Centre(known->GetBounds());
    } else {
      std::lock_guard<std::mutex> lock(m_mutex);
      std::weak_ptr<const geometry::AbstractShape> key = g;
      auto it = m_cache.find(key);
      if (it == m_cache.end()) {
        if (m_cache.size() > 100000) m_cache.clear();
        it = m_cache.emplace(key, Centre(BoundsOf(*g, 0.2))).first;
      }
      local = it->second;
    }
    return surface.shape.GetTransform().Apply(local);
  }

 private:
  std::mutex m_mutex;
  std::map<std::weak_ptr<const geometry::AbstractShape>, Vec3,
           std::owner_less<std::weak_ptr<const geometry::AbstractShape>>> m_cache;
};

using PointTest = std::function<bool(const Vec3&)>;

Selector Make(PointTest test, LineTest lines) {
  auto t = std::make_shared<const PointTest>(std::move(test));
  filter::Filter<data::PointElement> p = [t](const data::PointElement& e) { return (*t)(e.geometry.position); };
  filter::Filter<data::LineElement> l = [t, lines](const data::LineElement& e) {
    const auto& v = e.geometry.vertices;
    if (v.empty()) return false;
    switch (lines) {
      case LineTest::AnyVertex:
        for (const auto& x : v)
          if ((*t)(x)) return true;
        return false;
      case LineTest::AllVertices:
        for (const auto& x : v)
          if (!(*t)(x)) return false;
        return true;
      case LineTest::Centroid: {
        Vec3 c{0, 0, 0};
        for (const auto& x : v)
          for (int i = 0; i < 3; ++i) c[i] += x[i];
        for (int i = 0; i < 3; ++i) c[i] /= static_cast<double>(v.size());
        return (*t)(c);
      }
    }
    return false;
  };
  auto cache = std::make_shared<CentreCache>();
  filter::Filter<data::SurfaceElement> s = [t, cache](const data::SurfaceElement& e) {
    return (*t)(cache->Get(e.geometry));
  };
  return {std::move(p), std::move(l), std::move(s)};
}

double WorldCriterion(const geometry::Shape& shape, const Vec3& p) {
  return shape.Geometry().Criterion(shape.GetTransform().Inverse(p));
}

geometry::Shape World(ShapePtr shape) {
  if (!shape) throw std::invalid_argument("shape filter: shape must not be null");
  // geometry::Shape holds a non-const pointer; the shape is never modified through it.
  return geometry::Shape(std::const_pointer_cast<geometry::AbstractShape>(shape));
}

}  // namespace

Vec3 SurfaceCentre(const data::Surface& surface) {
  return surface.shape.GetTransform().Apply(Centre(BoundsOf(surface.shape.Geometry(), 0.2)));
}

Selector Inside(const geometry::Shape& shape, LineTest lines) {
  return Make([shape](const Vec3& p) { return WorldCriterion(shape, p) <= 0; }, lines);
}
Selector Inside(ShapePtr shape, LineTest lines) { return Inside(World(std::move(shape)), lines); }

Selector Outside(const geometry::Shape& shape, LineTest lines) {
  return Make([shape](const Vec3& p) { return WorldCriterion(shape, p) > 0; }, lines);
}
Selector Outside(ShapePtr shape, LineTest lines) { return Outside(World(std::move(shape)), lines); }

Selector Within(double distance, const geometry::Shape& shape, LineTest lines) {
  return Make([shape, distance](const Vec3& p) { return WorldCriterion(shape, p) <= distance; }, lines);
}
Selector Within(double distance, ShapePtr shape, LineTest lines) {
  return Within(distance, World(std::move(shape)), lines);
}

}  // namespace fccvis::stdlib
