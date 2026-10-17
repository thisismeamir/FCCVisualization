// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
#include "fccvis/stdlib/Shapes.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <queue>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace fccvis::stdlib {

namespace {

constexpr double kPi = std::numbers::pi;
constexpr double kNone = -std::numeric_limits<double>::infinity();  // "no constraint" in max()

using Mesh = geometry::Mesh;
using Tri = std::array<std::uint32_t, 3>;

double Dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 Sub(const Vec3& a, const Vec3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 Cross(const Vec3& a, const Vec3& b) {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
double Norm(const Vec3& a) { return std::sqrt(Dot(a, a)); }

bool FullTurn(double delta) { return delta >= kTwoPi - 1e-9; }

void CheckPhi(double delta) {
  if (!(delta > 0.0) || delta > kTwoPi + 1e-9)
    throw std::invalid_argument("phiDelta must be in (0, 2*pi]");
}

// Angle of (x, y) measured from phiStart, in [0, 2π).
double PhiFromStart(double x, double y, double start) {
  double d = std::fmod(std::atan2(y, x) - start, kTwoPi);
  if (d < 0) d += kTwoPi;
  return d;
}

// Signed distance-like term of a phi section (kNone for a full turn).
double PhiTerm(double x, double y, double start, double delta) {
  if (FullTurn(delta)) return kNone;
  const double r = std::hypot(x, y);
  const double d = PhiFromStart(x, y, start);
  auto toPlane = [r](double angle) { return angle >= kPi / 2 ? r : r * std::sin(angle); };
  if (d <= delta) return -std::min(toPlane(d), toPlane(delta - d));
  return toPlane(std::min(d - delta, kTwoPi - d));
}

// Bounds of a set of (r, z) profile points swept over a phi section.
Bounds SweepBounds(const std::vector<std::array<double, 2>>& profile, double start, double delta) {
  std::vector<double> angles;
  if (FullTurn(delta)) {
    angles = {0, kPi / 2, kPi, 3 * kPi / 2};
  } else {
    angles = {start, start + delta};
    for (int k = -4; k <= 8; ++k) {
      const double a = k * kPi / 2;
      double d = std::fmod(a - start, kTwoPi);
      if (d < 0) d += kTwoPi;
      if (d <= delta) angles.push_back(a);
    }
  }
  Bounds b;
  for (const auto& [r, z] : profile)
    for (double a : angles) b.Add({r * std::cos(a), r * std::sin(a), z});
  return b;
}

// ---- mesh utilities ---------------------------------------------------------

std::uint64_t EdgeKey(std::uint32_t a, std::uint32_t b) {
  return (static_cast<std::uint64_t>(std::min(a, b)) << 32) | std::max(a, b);
}

// Makes all triangles of each connected component wind the same way.
void OrientConsistently(Mesh& m) {
  const std::size_t n = m.triangles.size();
  std::unordered_map<std::uint64_t, std::vector<std::size_t>> byEdge;
  for (std::size_t t = 0; t < n; ++t)
    for (int e = 0; e < 3; ++e)
      byEdge[EdgeKey(m.triangles[t][e], m.triangles[t][(e + 1) % 3])].push_back(t);
  std::vector<char> seen(n, 0);
  auto hasDirected = [&](std::size_t t, std::uint32_t a, std::uint32_t b) {
    const auto& tr = m.triangles[t];
    for (int e = 0; e < 3; ++e)
      if (tr[e] == a && tr[(e + 1) % 3] == b) return true;
    return false;
  };
  for (std::size_t s = 0; s < n; ++s) {
    if (seen[s]) continue;
    seen[s] = 1;
    std::queue<std::size_t> q;
    q.push(s);
    while (!q.empty()) {
      const std::size_t t = q.front();
      q.pop();
      for (int e = 0; e < 3; ++e) {
        const std::uint32_t a = m.triangles[t][e], b = m.triangles[t][(e + 1) % 3];
        for (std::size_t u : byEdge[EdgeKey(a, b)]) {
          if (u == t || seen[u]) continue;
          if (hasDirected(u, a, b)) std::swap(m.triangles[u][1], m.triangles[u][2]);
          seen[u] = 1;
          q.push(u);
        }
      }
    }
  }
}

void FlipIfInward(Mesh& m) {
  if (SignedVolume(m) < 0)
    for (auto& t : m.triangles) std::swap(t[1], t[2]);
}

void AddTri(Mesh& m, std::uint32_t a, std::uint32_t b, std::uint32_t c) {
  if (a == b || b == c || a == c) return;
  m.triangles.push_back({a, b, c});
}

double Area2D(const std::vector<std::array<double, 2>>& p) {
  double a = 0;
  for (std::size_t i = 0; i < p.size(); ++i) {
    const auto& u = p[i];
    const auto& v = p[(i + 1) % p.size()];
    a += u[0] * v[1] - v[0] * u[1];
  }
  return a / 2;
}

// Ear clipping of a simple polygon; returns index triples into @p p.
std::vector<Tri> Triangulate(const std::vector<std::array<double, 2>>& p) {
  std::vector<Tri> out;
  std::vector<std::uint32_t> idx(p.size());
  for (std::uint32_t i = 0; i < p.size(); ++i) idx[i] = i;
  const double sign = Area2D(p) >= 0 ? 1.0 : -1.0;
  auto cross = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    return ((p[b][0] - p[a][0]) * (p[c][1] - p[a][1]) - (p[b][1] - p[a][1]) * (p[c][0] - p[a][0])) * sign;
  };
  auto inside = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t x) {
    return cross(a, b, x) >= 0 && cross(b, c, x) >= 0 && cross(c, a, x) >= 0;
  };
  std::size_t guard = 0;
  while (idx.size() > 3 && guard++ < 10000) {
    bool clipped = false;
    for (std::size_t i = 0; i < idx.size(); ++i) {
      const auto a = idx[(i + idx.size() - 1) % idx.size()], b = idx[i], c = idx[(i + 1) % idx.size()];
      if (cross(a, b, c) <= 1e-15) continue;
      bool ear = true;
      for (auto x : idx)
        if (x != a && x != b && x != c && inside(a, b, c, x)) { ear = false; break; }
      if (!ear) continue;
      out.push_back({a, b, c});
      idx.erase(idx.begin() + static_cast<std::ptrdiff_t>(i));
      clipped = true;
      break;
    }
    if (!clipped) {  // degenerate (collinear) remainder: fan it
      for (std::size_t i = 1; i + 1 < idx.size(); ++i) out.push_back({idx[0], idx[i], idx[i + 1]});
      return out;
    }
  }
  if (idx.size() == 3) out.push_back({idx[0], idx[1], idx[2]});
  return out;
}

// Revolves a closed (r, z) polygon around z over a phi section.
Mesh Revolve(std::vector<std::array<double, 2>> profile, int segmentsFullTurn, double start,
             double delta) {
  // clean: snap the axis, drop consecutive duplicates
  for (auto& p : profile)
    if (std::abs(p[0]) < 1e-12) p[0] = 0;
  std::vector<std::array<double, 2>> clean;
  for (const auto& p : profile)
    if (clean.empty() || std::abs(clean.back()[0] - p[0]) > 1e-12 || std::abs(clean.back()[1] - p[1]) > 1e-12)
      clean.push_back(p);
  while (clean.size() > 1 && std::abs(clean.back()[0] - clean.front()[0]) < 1e-12 &&
         std::abs(clean.back()[1] - clean.front()[1]) < 1e-12)
    clean.pop_back();
  Mesh m;
  if (clean.size() < 3) return m;

  const bool full = FullTurn(delta);
  const int steps = std::max(1, static_cast<int>(std::ceil(segmentsFullTurn * delta / kTwoPi)));
  const int positions = full ? steps : steps + 1;
  std::vector<std::vector<std::uint32_t>> ring(clean.size());
  for (std::size_t i = 0; i < clean.size(); ++i) {
    const auto [r, z] = clean[i];
    if (r == 0) {
      ring[i] = {static_cast<std::uint32_t>(m.vertices.size())};
      m.vertices.push_back({0.f, 0.f, static_cast<float>(z)});
      continue;
    }
    for (int k = 0; k < positions; ++k) {
      const double a = start + delta * k / steps;
      ring[i].push_back(static_cast<std::uint32_t>(m.vertices.size()));
      m.vertices.push_back({static_cast<float>(r * std::cos(a)), static_cast<float>(r * std::sin(a)),
                            static_cast<float>(z)});
    }
  }
  auto idx = [&](std::size_t i, int k) {
    if (ring[i].size() == 1) return ring[i][0];
    return ring[i][static_cast<std::size_t>(full ? k % positions : k)];
  };
  for (std::size_t i = 0; i < clean.size(); ++i) {
    const std::size_t j = (i + 1) % clean.size();
    if (ring[i].size() == 1 && ring[j].size() == 1) continue;  // edge on the axis
    for (int k = 0; k < steps; ++k) {
      const auto a = idx(i, k), b = idx(i, k + 1), c = idx(j, k + 1), d = idx(j, k);
      AddTri(m, a, b, c);
      AddTri(m, a, c, d);
    }
  }
  if (!full) {
    for (const auto& t : Triangulate(clean)) {
      AddTri(m, idx(t[0], 0), idx(t[1], 0), idx(t[2], 0));
      AddTri(m, idx(t[0], steps), idx(t[2], steps), idx(t[1], steps));
    }
  }
  OrientConsistently(m);
  FlipIfInward(m);
  return m;
}

Vec3 CornerOf(const Bounds& b, int i) {
  return {(i & 1) ? b.hi[0] : b.lo[0], (i & 2) ? b.hi[1] : b.lo[1], (i & 4) ? b.hi[2] : b.lo[2]};
}

}  // namespace

// ---- Bounds and mesh helpers -------------------------------------------------

void Bounds::Add(const Vec3& p) {
  if (Empty()) {
    lo = hi = p;
    return;
  }
  for (int i = 0; i < 3; ++i) {
    lo[i] = std::min(lo[i], p[i]);
    hi[i] = std::max(hi[i], p[i]);
  }
}

Bounds Bounds::Union(const Bounds& o) const {
  if (Empty()) return o;
  if (o.Empty()) return *this;
  Bounds b = *this;
  b.Add(o.lo);
  b.Add(o.hi);
  return b;
}

Bounds Bounds::Intersection(const Bounds& o) const {
  Bounds b;
  for (int i = 0; i < 3; ++i) {
    b.lo[i] = std::max(lo[i], o.lo[i]);
    b.hi[i] = std::min(hi[i], o.hi[i]);
  }
  return b;
}

double Bounds::Diagonal() const { return Empty() ? 0.0 : Norm(Sub(hi, lo)); }

Bounds BoundsOf(const geometry::Mesh& mesh) {
  Bounds b;
  for (const auto& v : mesh.vertices) b.Add({v[0], v[1], v[2]});
  return b;
}

Bounds BoundsOf(const geometry::AbstractShape& shape, double tolerance) {
  if (const auto* s = dynamic_cast<const StandardShape*>(&shape)) return s->GetBounds();
  return BoundsOf(shape.Tessellate(tolerance));
}

bool IsClosed(const geometry::Mesh& mesh) {
  if (mesh.triangles.empty()) return false;
  std::map<std::pair<std::uint32_t, std::uint32_t>, int> directed;
  for (const auto& t : mesh.triangles)
    for (int e = 0; e < 3; ++e)
      if (++directed[{t[e], t[(e + 1) % 3]}] > 1) return false;
  for (const auto& [edge, n] : directed)
    if (!directed.count({edge.second, edge.first})) return false;
  return true;
}

double SignedVolume(const geometry::Mesh& mesh) {
  double v = 0;
  for (const auto& t : mesh.triangles) {
    const auto& a = mesh.vertices[t[0]];
    const auto& b = mesh.vertices[t[1]];
    const auto& c = mesh.vertices[t[2]];
    const Vec3 A{a[0], a[1], a[2]}, B{b[0], b[1], b[2]}, C{c[0], c[1], c[2]};
    v += Dot(A, Cross(B, C));
  }
  return v / 6.0;
}

int SegmentsForTolerance(double tolerance) {
  const double t = std::clamp(tolerance, 1e-4, 0.2);
  return std::clamp(static_cast<int>(std::ceil(kPi / std::acos(1.0 - t))), 8, 360);
}

// ---- Box ---------------------------------------------------------------------

Box::Box(double dx, double dy, double dz) : m_half{dx, dy, dz} {
  if (!(dx > 0 && dy > 0 && dz > 0)) throw std::invalid_argument("Box half-lengths must be positive");
}
std::shared_ptr<geometry::AbstractShape> Box::Clone() const { return std::make_shared<Box>(*this); }
double Box::Criterion(const Vector& p) const {
  Vec3 q;
  double outside = 0, maxq = kNone;
  for (int i = 0; i < 3; ++i) {
    q[i] = std::abs(p[i]) - m_half[i];
    outside += std::max(q[i], 0.0) * std::max(q[i], 0.0);
    maxq = std::max(maxq, q[i]);
  }
  return maxq > 0 ? std::sqrt(outside) : maxq;
}
geometry::Mesh Box::Tessellate(double) const {
  Mesh m;
  for (int i = 0; i < 8; ++i)
    m.vertices.push_back({static_cast<float>((i & 1) ? m_half[0] : -m_half[0]),
                          static_cast<float>((i & 2) ? m_half[1] : -m_half[1]),
                          static_cast<float>((i & 4) ? m_half[2] : -m_half[2])});
  const int faces[6][4] = {{0, 2, 3, 1}, {4, 5, 7, 6}, {0, 1, 5, 4}, {2, 6, 7, 3}, {0, 4, 6, 2}, {1, 3, 7, 5}};
  for (const auto& f : faces) {
    AddTri(m, f[0], f[1], f[2]);
    AddTri(m, f[0], f[2], f[3]);
  }
  OrientConsistently(m);
  FlipIfInward(m);
  return m;
}
Bounds Box::GetBounds() const { return {{-m_half[0], -m_half[1], -m_half[2]}, m_half}; }

// ---- Sphere ------------------------------------------------------------------

Sphere::Sphere(double r) : m_r(r) {
  if (!(r > 0)) throw std::invalid_argument("Sphere radius must be positive");
}
std::shared_ptr<geometry::AbstractShape> Sphere::Clone() const { return std::make_shared<Sphere>(*this); }
double Sphere::Criterion(const Vector& p) const { return Norm(p) - m_r; }
geometry::Mesh Sphere::Tessellate(double tolerance) const {
  const int seg = SegmentsForTolerance(tolerance);
  const int m = std::max(4, seg / 2);
  std::vector<std::array<double, 2>> profile{{0.0, -m_r}};
  for (int i = 1; i < m; ++i) {
    const double t = -kPi / 2 + kPi * i / m;
    profile.push_back({m_r * std::cos(t), m_r * std::sin(t)});
  }
  profile.push_back({0.0, m_r});
  return Revolve(profile, seg, 0.0, kTwoPi);
}
Bounds Sphere::GetBounds() const { return {{-m_r, -m_r, -m_r}, {m_r, m_r, m_r}}; }

// ---- Cone / Cylinder ---------------------------------------------------------

Cone::Cone(double rMin1, double rMax1, double rMin2, double rMax2, double halfZ, double phiStart,
           double phiDelta)
    : m_rMin1(rMin1), m_rMax1(rMax1), m_rMin2(rMin2), m_rMax2(rMax2), m_halfZ(halfZ),
      m_phiStart(phiStart), m_phiDelta(phiDelta) {
  if (rMin1 < 0 || rMin2 < 0 || rMin1 > rMax1 || rMin2 > rMax2 || (rMax1 <= 0 && rMax2 <= 0))
    throw std::invalid_argument("Cone radii must satisfy 0 <= rMin <= rMax and not all zero");
  if (!(halfZ > 0)) throw std::invalid_argument("Cone halfZ must be positive");
  CheckPhi(phiDelta);
}
std::shared_ptr<geometry::AbstractShape> Cone::Clone() const { return std::make_shared<Cone>(*this); }
double Cone::Criterion(const Vector& p) const {
  const double r = std::hypot(p[0], p[1]);
  const double t = std::clamp((p[2] + m_halfZ) / (2 * m_halfZ), 0.0, 1.0);
  const double rmin = m_rMin1 + (m_rMin2 - m_rMin1) * t;
  const double rmax = m_rMax1 + (m_rMax2 - m_rMax1) * t;
  const double cosOut = std::cos(std::atan((m_rMax2 - m_rMax1) / (2 * m_halfZ)));
  const double cosIn = std::cos(std::atan((m_rMin2 - m_rMin1) / (2 * m_halfZ)));
  double d = std::max((r - rmax) * cosOut, std::abs(p[2]) - m_halfZ);
  if (m_rMin1 > 0 || m_rMin2 > 0) d = std::max(d, (rmin - r) * cosIn);
  return std::max(d, PhiTerm(p[0], p[1], m_phiStart, m_phiDelta));
}
geometry::Mesh Cone::Tessellate(double tolerance) const {
  return Revolve({{m_rMin1, -m_halfZ}, {m_rMax1, -m_halfZ}, {m_rMax2, m_halfZ}, {m_rMin2, m_halfZ}},
                 SegmentsForTolerance(tolerance), m_phiStart, m_phiDelta);
}
Bounds Cone::GetBounds() const {
  return SweepBounds({{m_rMin1, -m_halfZ}, {m_rMax1, -m_halfZ}, {m_rMax2, m_halfZ}, {m_rMin2, m_halfZ}},
                     m_phiStart, m_phiDelta);
}

Cylinder::Cylinder(double rMin, double rMax, double halfZ, double phiStart, double phiDelta)
    : Cone(rMin, rMax, rMin, rMax, halfZ, phiStart, phiDelta) {}
std::shared_ptr<geometry::AbstractShape> Cylinder::Clone() const { return std::make_shared<Cylinder>(*this); }

// ---- Polycone ----------------------------------------------------------------

Polycone::Polycone(std::vector<double> z, std::vector<double> rMin, std::vector<double> rMax,
                   double phiStart, double phiDelta)
    : m_z(std::move(z)), m_rMin(std::move(rMin)), m_rMax(std::move(rMax)), m_phiStart(phiStart),
      m_phiDelta(phiDelta) {
  if (m_z.size() < 2 || m_rMin.size() != m_z.size() || m_rMax.size() != m_z.size())
    throw std::invalid_argument("Polycone needs at least two planes and one rMin/rMax per plane");
  for (std::size_t i = 0; i < m_z.size(); ++i) {
    if (i && m_z[i] < m_z[i - 1]) throw std::invalid_argument("Polycone z planes must not decrease");
    if (m_rMin[i] < 0 || m_rMin[i] > m_rMax[i]) throw std::invalid_argument("Polycone needs 0 <= rMin <= rMax");
  }
  if (!(m_z.back() > m_z.front())) throw std::invalid_argument("Polycone must have a positive length");
  CheckPhi(phiDelta);
}
std::shared_ptr<geometry::AbstractShape> Polycone::Clone() const { return std::make_shared<Polycone>(*this); }
double Polycone::Criterion(const Vector& p) const {
  const double r = std::hypot(p[0], p[1]);
  const double z = p[2];
  double d = std::max(m_z.front() - z, z - m_z.back());
  const double zc = std::clamp(z, m_z.front(), m_z.back());
  // radial term of the segment containing zc (widest if zc sits on a step)
  double best = std::numeric_limits<double>::infinity();
  for (std::size_t i = 0; i + 1 < m_z.size(); ++i) {
    if (zc < m_z[i] || zc > m_z[i + 1] || m_z[i + 1] == m_z[i]) continue;
    const double t = (zc - m_z[i]) / (m_z[i + 1] - m_z[i]);
    const double rmin = m_rMin[i] + (m_rMin[i + 1] - m_rMin[i]) * t;
    const double rmax = m_rMax[i] + (m_rMax[i + 1] - m_rMax[i]) * t;
    double radial = r - rmax;
    if (rmin > 0) radial = std::max(radial, rmin - r);
    best = std::min(best, radial);
  }
  d = std::max(d, best);
  return std::max(d, PhiTerm(p[0], p[1], m_phiStart, m_phiDelta));
}
geometry::Mesh Polycone::Tessellate(double tolerance) const {
  std::vector<std::array<double, 2>> profile;
  for (std::size_t i = 0; i < m_z.size(); ++i) profile.push_back({m_rMax[i], m_z[i]});
  for (std::size_t i = m_z.size(); i-- > 0;) profile.push_back({m_rMin[i], m_z[i]});
  return Revolve(profile, SegmentsForTolerance(tolerance), m_phiStart, m_phiDelta);
}
Bounds Polycone::GetBounds() const {
  std::vector<std::array<double, 2>> profile;
  for (std::size_t i = 0; i < m_z.size(); ++i) {
    profile.push_back({m_rMax[i], m_z[i]});
    profile.push_back({m_rMin[i], m_z[i]});
  }
  return SweepBounds(profile, m_phiStart, m_phiDelta);
}

// ---- HalfSpace ---------------------------------------------------------------

HalfSpace::HalfSpace(Vec3 normal, double offset, double extent) : m_offset(offset), m_extent(extent) {
  const double n = Norm(normal);
  if (!(n > 0)) throw std::invalid_argument("HalfSpace normal must not be zero");
  if (!(extent > 0)) throw std::invalid_argument("HalfSpace extent must be positive");
  m_n = {normal[0] / n, normal[1] / n, normal[2] / n};
}
std::shared_ptr<geometry::AbstractShape> HalfSpace::Clone() const { return std::make_shared<HalfSpace>(*this); }
double HalfSpace::Criterion(const Vector& p) const { return Dot(m_n, p) - m_offset; }
geometry::Mesh HalfSpace::Tessellate(double) const {
  const Vec3 helper = std::abs(m_n[0]) < 0.9 ? Vec3{1, 0, 0} : Vec3{0, 1, 0};
  Vec3 u = Cross(m_n, helper);
  const double un = Norm(u);
  u = {u[0] / un, u[1] / un, u[2] / un};
  const Vec3 v = Cross(m_n, u);
  const Vec3 c{m_n[0] * m_offset, m_n[1] * m_offset, m_n[2] * m_offset};
  Mesh m;
  for (const auto& [a, b] : std::array<std::array<double, 2>, 4>{{{-1, -1}, {1, -1}, {1, 1}, {-1, 1}}})
    m.vertices.push_back({static_cast<float>(c[0] + m_extent * (a * u[0] + b * v[0])),
                          static_cast<float>(c[1] + m_extent * (a * u[1] + b * v[1])),
                          static_cast<float>(c[2] + m_extent * (a * u[2] + b * v[2]))});
  m.triangles = {{0, 1, 2}, {0, 2, 3}};  // (u, v, n) is right-handed: normal along n
  return m;
}
Bounds HalfSpace::GetBounds() const {
  return {{-m_extent, -m_extent, -m_extent}, {m_extent, m_extent, m_extent}};
}

// ---- Wedge -------------------------------------------------------------------

Wedge::Wedge(double phiStart, double phiDelta, double thetaMin, double thetaMax, double rMax)
    : m_phiStart(phiStart), m_phiDelta(phiDelta), m_thetaMin(thetaMin), m_thetaMax(thetaMax), m_rMax(rMax) {
  CheckPhi(phiDelta);
  if (!(thetaMin >= 0 && thetaMax <= kPi && thetaMin < thetaMax))
    throw std::invalid_argument("Wedge needs 0 <= thetaMin < thetaMax <= pi");
  if (!(rMax > 0)) throw std::invalid_argument("Wedge rMax must be positive");
}
std::shared_ptr<geometry::AbstractShape> Wedge::Clone() const { return std::make_shared<Wedge>(*this); }
double Wedge::Criterion(const Vector& p) const {
  const double r = Norm(p);
  if (r == 0) return -0.0;
  const double theta = std::acos(std::clamp(p[2] / r, -1.0, 1.0));
  double dTheta = std::max(m_thetaMin - theta, theta - m_thetaMax);
  dTheta = dTheta > 0 ? r * std::sin(std::min(dTheta, kPi / 2)) : r * std::max(dTheta, -kPi / 2);
  double d = std::max(r - m_rMax, dTheta);
  return std::max(d, PhiTerm(p[0], p[1], m_phiStart, m_phiDelta));
}
geometry::Mesh Wedge::Tessellate(double tolerance) const {
  const int seg = SegmentsForTolerance(tolerance);
  const int m = std::max(2, static_cast<int>(std::ceil(seg * (m_thetaMax - m_thetaMin) / kTwoPi)));
  std::vector<std::array<double, 2>> profile{{0.0, 0.0}};
  for (int i = 0; i <= m; ++i) {
    const double t = m_thetaMin + (m_thetaMax - m_thetaMin) * i / m;
    profile.push_back({m_rMax * std::sin(t), m_rMax * std::cos(t)});
  }
  return Revolve(profile, seg, m_phiStart, m_phiDelta);
}
Bounds Wedge::GetBounds() const {
  std::vector<std::array<double, 2>> profile{{0.0, 0.0}};
  for (double t : {m_thetaMin, m_thetaMax}) profile.push_back({m_rMax * std::sin(t), m_rMax * std::cos(t)});
  if (m_thetaMin < kPi / 2 && m_thetaMax > kPi / 2) profile.push_back({m_rMax, 0.0});
  return SweepBounds(profile, m_phiStart, m_phiDelta);
}

// ---- Booleans ----------------------------------------------------------------

BooleanShape::BooleanShape(ShapePtr a, ShapePtr b) : m_a(std::move(a)), m_b(std::move(b)) {
  if (!m_a || !m_b) throw std::invalid_argument("boolean shape operands must not be null");
}
geometry::Mesh BooleanShape::Tessellate(double tolerance) const {
  const Bounds b = GetBounds();
  if (b.Empty()) return {};
  const double cell = std::max(std::clamp(tolerance, 1e-3, 0.5) * b.Diagonal(), 1e-6);
  return MarchTetrahedra(*this, b, cell);
}

std::shared_ptr<geometry::AbstractShape> Union::Clone() const { return std::make_shared<Union>(*this); }
double Union::Criterion(const Vector& p) const { return std::min(m_a->Criterion(p), m_b->Criterion(p)); }
Bounds Union::GetBounds() const { return BoundsOf(*m_a).Union(BoundsOf(*m_b)); }

std::shared_ptr<geometry::AbstractShape> Intersection::Clone() const { return std::make_shared<Intersection>(*this); }
double Intersection::Criterion(const Vector& p) const { return std::max(m_a->Criterion(p), m_b->Criterion(p)); }
Bounds Intersection::GetBounds() const { return BoundsOf(*m_a).Intersection(BoundsOf(*m_b)); }

std::shared_ptr<geometry::AbstractShape> Difference::Clone() const { return std::make_shared<Difference>(*this); }
double Difference::Criterion(const Vector& p) const { return std::max(m_a->Criterion(p), -m_b->Criterion(p)); }
Bounds Difference::GetBounds() const { return BoundsOf(*m_a); }

// ---- Transformed -------------------------------------------------------------

Transformed::Transformed(ShapePtr shape, geometry::Transform transform)
    : m_shape(std::move(shape)), m_transform(std::move(transform)) {
  if (!m_shape) throw std::invalid_argument("Transformed shape must not be null");
}
std::shared_ptr<geometry::AbstractShape> Transformed::Clone() const { return std::make_shared<Transformed>(*this); }
double Transformed::Criterion(const Vector& p) const { return m_shape->Criterion(m_transform.Inverse(p)); }
geometry::Mesh Transformed::Tessellate(double tolerance) const {
  Mesh m = m_shape->Tessellate(tolerance);
  for (auto& v : m.vertices) {
    const auto w = m_transform.Apply({v[0], v[1], v[2]});
    v = {static_cast<float>(w[0]), static_cast<float>(w[1]), static_cast<float>(w[2])};
  }
  const auto& R = m_transform.Rotation();
  const double det = R[0] * (R[4] * R[8] - R[5] * R[7]) - R[1] * (R[3] * R[8] - R[5] * R[6]) +
                     R[2] * (R[3] * R[7] - R[4] * R[6]);
  if (det < 0)
    for (auto& t : m.triangles) std::swap(t[1], t[2]);
  return m;
}
Bounds Transformed::GetBounds() const {
  const Bounds child = BoundsOf(*m_shape);
  Bounds b;
  if (child.Empty()) return b;
  for (int i = 0; i < 8; ++i) b.Add(m_transform.Apply(CornerOf(child, i)));
  return b;
}

// ---- Marching tetrahedra -----------------------------------------------------

geometry::Mesh MarchTetrahedra(const geometry::AbstractShape& shape, const Bounds& bounds, double cell) {
  Mesh m;
  if (bounds.Empty() || !(cell > 0)) return m;
  std::array<int, 3> n{};
  std::array<double, 3> h{}, lo{};
  for (int a = 0; a < 3; ++a) {
    lo[a] = bounds.lo[a] - cell;
    const double ext = bounds.hi[a] + cell - lo[a];
    n[a] = std::clamp(static_cast<int>(std::ceil(ext / cell)), 2, 128);
    h[a] = ext / n[a];
  }
  const int nx = n[0] + 1, ny = n[1] + 1, nz = n[2] + 1;
  auto gid = [&](int i, int j, int k) { return static_cast<std::uint32_t>((k * ny + j) * nx + i); };
  auto pos = [&](std::uint32_t g) -> Vec3 {
    const int i = static_cast<int>(g % nx), j = static_cast<int>((g / nx) % ny), k = static_cast<int>(g / (nx * ny));
    return {lo[0] + i * h[0], lo[1] + j * h[1], lo[2] + k * h[2]};
  };
  std::vector<double> f(static_cast<std::size_t>(nx) * ny * nz);
  for (int k = 0; k < nz; ++k)
    for (int j = 0; j < ny; ++j)
      for (int i = 0; i < nx; ++i) f[gid(i, j, k)] = shape.Criterion(pos(gid(i, j, k)));

  std::unordered_map<std::uint64_t, std::uint32_t> edgeVertex;
  auto vertexOn = [&](std::uint32_t a, std::uint32_t b) {
    const auto key = EdgeKey(a, b);
    auto it = edgeVertex.find(key);
    if (it != edgeVertex.end()) return it->second;
    const double fa = f[a], fb = f[b];
    const double t = fa == fb ? 0.5 : fa / (fa - fb);
    const Vec3 pa = pos(a), pb = pos(b);
    const auto id = static_cast<std::uint32_t>(m.vertices.size());
    m.vertices.push_back({static_cast<float>(pa[0] + t * (pb[0] - pa[0])),
                          static_cast<float>(pa[1] + t * (pb[1] - pa[1])),
                          static_cast<float>(pa[2] + t * (pb[2] - pa[2]))});
    edgeVertex.emplace(key, id);
    return id;
  };
  auto mid = [&](std::uint32_t a, std::uint32_t b) {
    const Vec3 pa = pos(a), pb = pos(b);
    return Vec3{(pa[0] + pb[0]) / 2, (pa[1] + pb[1]) / 2, (pa[2] + pb[2]) / 2};
  };
  // Emits the triangle through edges e0, e1, e2, oriented away from `outside`
  // (orientation decided on edge midpoints, which never degenerate).
  using Edge = std::pair<std::uint32_t, std::uint32_t>;
  auto emit = [&](Edge e0, Edge e1, Edge e2, std::uint32_t outside) {
    const Vec3 m0 = mid(e0.first, e0.second), m1 = mid(e1.first, e1.second), m2 = mid(e2.first, e2.second);
    const Vec3 nrm = Cross(Sub(m1, m0), Sub(m2, m0));
    const Vec3 c{(m0[0] + m1[0] + m2[0]) / 3, (m0[1] + m1[1] + m2[1]) / 3, (m0[2] + m1[2] + m2[2]) / 3};
    const bool flip = Dot(nrm, Sub(pos(outside), c)) < 0;
    const auto a = vertexOn(e0.first, e0.second);
    const auto b = vertexOn(e1.first, e1.second);
    const auto d = vertexOn(e2.first, e2.second);
    if (flip) AddTri(m, a, d, b); else AddTri(m, a, b, d);
  };
  static constexpr int kTets[6][4] = {{0, 1, 3, 7}, {0, 1, 5, 7}, {0, 2, 3, 7},
                                      {0, 2, 6, 7}, {0, 4, 5, 7}, {0, 4, 6, 7}};
  for (int k = 0; k + 1 < nz; ++k)
    for (int j = 0; j + 1 < ny; ++j)
      for (int i = 0; i + 1 < nx; ++i) {
        std::uint32_t c[8];
        for (int v = 0; v < 8; ++v) c[v] = gid(i + (v & 1), j + ((v >> 1) & 1), k + ((v >> 2) & 1));
        for (const auto& tet : kTets) {
          std::vector<std::uint32_t> in, out;
          for (int v : tet) (f[c[v]] <= 0 ? in : out).push_back(c[v]);
          if (in.empty() || out.empty()) continue;
          if (in.size() == 1) {
            emit({in[0], out[0]}, {in[0], out[1]}, {in[0], out[2]}, out[0]);
          } else if (in.size() == 3) {
            emit({out[0], in[0]}, {out[0], in[1]}, {out[0], in[2]}, out[0]);
          } else {
            emit({in[0], out[0]}, {in[0], out[1]}, {in[1], out[1]}, out[0]);
            emit({in[0], out[0]}, {in[1], out[1]}, {in[1], out[0]}, out[0]);
          }
        }
      }
  return m;
}

}  // namespace fccvis::stdlib
