/**
 * @file Shapes.h
 * @brief Standard shapes: AbstractShape implementations for selections and geometry.
 *
 * Every shape implements the three operations of geometry::AbstractShape:
 * - `Clone()`: deep copy;
 * - `Criterion(p)`: approximate signed distance in millimetres, <= 0 inside,
 *   > 0 outside (the sign is exact; the magnitude is a distance estimate used
 *   by `Within` and by boolean shapes);
 * - `Tessellate(tolerance)`: closed, consistently oriented (outward) triangle
 *   mesh in the shape's local frame. `tolerance` is relative: the chord
 *   sagitta of curved surfaces is at most `tolerance` times the radius, and
 *   boolean shapes use cells of `tolerance` times their bounding-box diagonal.
 *
 * Angles are radians; lengths are millimetres. All shapes are centred on the
 * local origin and aligned with the local z axis; place them with
 * Transformed or geometry::Shape.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include "Geometry.h"

#include <array>
#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

namespace fccvis::stdlib {

/** @brief Point or vector in millimetres. */
using Vec3 = geometry::AbstractShape::Vector;

/** @brief Full turn in radians. */
inline constexpr double kTwoPi = 2.0 * std::numbers::pi;

/** @brief Axis-aligned bounding box. Empty when lo > hi on any axis. */
struct Bounds {
  Vec3 lo{1, 1, 1};    ///< Minimum corner.
  Vec3 hi{-1, -1, -1}; ///< Maximum corner.

  /** @brief True if the box contains no point. */
  [[nodiscard]] bool Empty() const { return lo[0] > hi[0] || lo[1] > hi[1] || lo[2] > hi[2]; }
  /** @brief Grows the box to contain @p p. */
  void Add(const Vec3& p);
  /** @brief Smallest box containing both. */
  [[nodiscard]] Bounds Union(const Bounds& o) const;
  /** @brief Overlap of both (may be empty). */
  [[nodiscard]] Bounds Intersection(const Bounds& o) const;
  /** @brief Length of the diagonal (0 for an empty box). */
  [[nodiscard]] double Diagonal() const;
};

/** @brief Bounding box of a mesh's vertices. */
[[nodiscard]] Bounds BoundsOf(const geometry::Mesh& mesh);

/**
 * @brief Bounding box of any shape in its local frame.
 *
 * Standard shapes answer exactly; other shapes (for example a TGeo shape) are
 * tessellated at @p tolerance and the mesh is measured.
 */
[[nodiscard]] Bounds BoundsOf(const geometry::AbstractShape& shape, double tolerance = 0.05);

/**
 * @brief True if every edge is shared by exactly two triangles with opposite
 *        directions (closed, consistently oriented 2-manifold).
 */
[[nodiscard]] bool IsClosed(const geometry::Mesh& mesh);

/** @brief Signed volume enclosed by a closed mesh (positive for outward orientation). */
[[nodiscard]] double SignedVolume(const geometry::Mesh& mesh);

/** @brief Number of segments per full turn for a relative sagitta tolerance (8..360). */
[[nodiscard]] int SegmentsForTolerance(double tolerance);

/**
 * @brief Base of the standard shapes: an AbstractShape that knows its bounds.
 */
class StandardShape : public geometry::AbstractShape {
 public:
  /** @brief Exact local bounding box. */
  [[nodiscard]] virtual Bounds GetBounds() const = 0;
};

/** @brief Shared, immutable shape handle used by composite shapes. */
using ShapePtr = std::shared_ptr<const geometry::AbstractShape>;

/** @brief Box with half-lengths @p dx, @p dy, @p dz. */
class Box final : public StandardShape {
 public:
  /** @throws std::invalid_argument if a half-length is not positive. */
  Box(double dx, double dy, double dz);
  std::shared_ptr<geometry::AbstractShape> Clone() const override;
  double Criterion(const Vector& p) const override;
  geometry::Mesh Tessellate(double tolerance) const override;
  Bounds GetBounds() const override;
 private:
  Vec3 m_half;
};

/** @brief Solid sphere of radius @p r. */
class Sphere final : public StandardShape {
 public:
  /** @throws std::invalid_argument if the radius is not positive. */
  explicit Sphere(double r);
  std::shared_ptr<geometry::AbstractShape> Clone() const override;
  double Criterion(const Vector& p) const override;
  geometry::Mesh Tessellate(double tolerance) const override;
  Bounds GetBounds() const override;
 private:
  double m_r;
};

/**
 * @brief Cone frustum, optionally hollow and limited to a phi section.
 *
 * Radii are linear in z between the planes z = -halfZ (rMin1, rMax1) and
 * z = +halfZ (rMin2, rMax2). The phi section starts at @p phiStart and spans
 * @p phiDelta (2π means the full turn).
 */
class Cone : public StandardShape {
 public:
  /** @throws std::invalid_argument for negative radii, rMin > rMax, halfZ <= 0 or phiDelta outside (0, 2π]. */
  Cone(double rMin1, double rMax1, double rMin2, double rMax2, double halfZ,
       double phiStart = 0.0, double phiDelta = kTwoPi);
  std::shared_ptr<geometry::AbstractShape> Clone() const override;
  double Criterion(const Vector& p) const override;
  geometry::Mesh Tessellate(double tolerance) const override;
  Bounds GetBounds() const override;
 protected:
  double m_rMin1;     ///< Inner radius at -halfZ.
  double m_rMax1;     ///< Outer radius at -halfZ.
  double m_rMin2;     ///< Inner radius at +halfZ.
  double m_rMax2;     ///< Outer radius at +halfZ.
  double m_halfZ;     ///< Half-length along z.
  double m_phiStart;  ///< Start of the phi section.
  double m_phiDelta;  ///< Width of the phi section.
};

/**
 * @brief Cylinder: full (rMin = 0) or hollow, optionally a phi section.
 *
 * A Cone with equal radii at both ends.
 */
class Cylinder final : public Cone {
 public:
  /** @copydoc Cone::Cone */
  Cylinder(double rMin, double rMax, double halfZ, double phiStart = 0.0,
           double phiDelta = kTwoPi);
  std::shared_ptr<geometry::AbstractShape> Clone() const override;
};

/** @brief Tube segment: a hollow cylinder limited to a phi section (same type as Cylinder). */
using TubeSegment = Cylinder;

/**
 * @brief Polycone: radii given at a sequence of z planes, optional phi section.
 *
 * Between consecutive planes the inner and outer radii are linear in z.
 * Consecutive planes may share a z value to describe a step.
 */
class Polycone final : public StandardShape {
 public:
  /**
   * @param z    Plane positions, non-decreasing, at least two distinct values.
   * @param rMin Inner radius at each plane.
   * @param rMax Outer radius at each plane (>= rMin).
   * @param phiStart Start of the phi section.
   * @param phiDelta Span of the phi section (2π for the full turn).
   * @throws std::invalid_argument on inconsistent input.
   */
  Polycone(std::vector<double> z, std::vector<double> rMin, std::vector<double> rMax,
           double phiStart = 0.0, double phiDelta = kTwoPi);
  std::shared_ptr<geometry::AbstractShape> Clone() const override;
  double Criterion(const Vector& p) const override;
  geometry::Mesh Tessellate(double tolerance) const override;
  Bounds GetBounds() const override;
 private:
  std::vector<double> m_z, m_rMin, m_rMax;
  double m_phiStart, m_phiDelta;
};

/**
 * @brief Half-space {p : n·p <= offset}, also usable as a cutting plane.
 *
 * The half-space is infinite: its tessellation is a single square patch of the
 * plane with side 2·extent (not a closed mesh), and its bounds are the cube of
 * half-side @p extent. Intersect it with a finite shape to get a closed cut.
 */
class HalfSpace final : public StandardShape {
 public:
  /**
   * @param normal Outward normal (normalised internally; must not be zero).
   * @param offset Signed distance of the plane from the origin along the normal.
   * @param extent Half-size of the patch/bounds used for drawing.
   */
  HalfSpace(Vec3 normal, double offset, double extent = 1000.0);
  std::shared_ptr<geometry::AbstractShape> Clone() const override;
  double Criterion(const Vector& p) const override;
  geometry::Mesh Tessellate(double tolerance) const override;
  Bounds GetBounds() const override;
 private:
  Vec3 m_n;
  double m_offset;
  double m_extent;
};

/**
 * @brief Spherical sector: r <= rMax, theta in [thetaMin, thetaMax], phi in the section.
 *
 * Theta is the polar angle from +z (0..π). With the full theta range and the
 * full phi turn it is a sphere.
 */
class Wedge final : public StandardShape {
 public:
  /** @throws std::invalid_argument on an empty or out-of-range sector. */
  Wedge(double phiStart, double phiDelta, double thetaMin, double thetaMax, double rMax);
  std::shared_ptr<geometry::AbstractShape> Clone() const override;
  double Criterion(const Vector& p) const override;
  geometry::Mesh Tessellate(double tolerance) const override;
  Bounds GetBounds() const override;
 private:
  double m_phiStart, m_phiDelta, m_thetaMin, m_thetaMax, m_rMax;
};

/**
 * @brief Base of boolean shapes. Membership comes from the children's
 *        criteria; tessellation marches tetrahedra on a grid over the bounds.
 */
class BooleanShape : public StandardShape {
 public:
  /** @throws std::invalid_argument if a child is null. */
  BooleanShape(ShapePtr a, ShapePtr b);
  geometry::Mesh Tessellate(double tolerance) const override;
 protected:
  ShapePtr m_a;  ///< First operand.
  ShapePtr m_b;  ///< Second operand.
};

/** @brief Points inside @p a or @p b. */
class Union final : public BooleanShape {
 public:
  using BooleanShape::BooleanShape;
  std::shared_ptr<geometry::AbstractShape> Clone() const override;
  double Criterion(const Vector& p) const override;
  Bounds GetBounds() const override;
};

/** @brief Points inside both @p a and @p b. */
class Intersection final : public BooleanShape {
 public:
  using BooleanShape::BooleanShape;
  std::shared_ptr<geometry::AbstractShape> Clone() const override;
  double Criterion(const Vector& p) const override;
  Bounds GetBounds() const override;
};

/** @brief Points inside @p a and not inside @p b. */
class Difference final : public BooleanShape {
 public:
  using BooleanShape::BooleanShape;
  std::shared_ptr<geometry::AbstractShape> Clone() const override;
  double Criterion(const Vector& p) const override;
  Bounds GetBounds() const override;
};

/** @brief A shape moved by a rigid transform (local → parent frame). */
class Transformed final : public StandardShape {
 public:
  /** @throws std::invalid_argument if @p shape is null. */
  Transformed(ShapePtr shape, geometry::Transform transform);
  std::shared_ptr<geometry::AbstractShape> Clone() const override;
  double Criterion(const Vector& p) const override;
  geometry::Mesh Tessellate(double tolerance) const override;
  Bounds GetBounds() const override;
 private:
  ShapePtr m_shape;
  geometry::Transform m_transform;
};

/**
 * @brief Marching-tetrahedra surface of any criterion over a box.
 *
 * Used by boolean shapes; exposed for shapes defined only by a criterion.
 * @param shape    Shape whose zero level set is extracted.
 * @param bounds   Region to sample (padded by one cell).
 * @param cell     Cell size in millimetres (the grid is capped at 128 cells per axis).
 * @return Closed, outward-oriented mesh (empty if nothing is inside).
 */
[[nodiscard]] geometry::Mesh MarchTetrahedra(const geometry::AbstractShape& shape,
                                             const Bounds& bounds, double cell);

}  // namespace fccvis::stdlib
