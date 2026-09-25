#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fccvis::geometry {

struct Mesh {
  std::vector<std::array<float, 3>> vertices;   // local frame
  std::vector<std::array<uint32_t, 3>> triangles; // indices into vertices
};

class AbstractShape {
public:
  using Vector = std::array<double, 3>;

  virtual ~AbstractShape() = default;
  virtual std::shared_ptr<AbstractShape> Clone() const = 0;

  /// <= 0 inside, > 0 outside (local frame).
  virtual double Criterion(const Vector& p) const = 0;

  /// Triangulated surface in the local frame.
  virtual Mesh Tessellate(double tolerance) const = 0;
};

/**
 * @brief Rigid transformation between coordinate systems.
 *
 * Represents translation and rotation of a geometric object.
 */
class Transform {
public:
  using Vector = std::array<double, 3>;
  using Matrix = std::array<double, 9>; // row-major 3 x 3 matrix

  Transform() = default;

  /**
   * @brief Construct a transform from translation and rotation.
   *
   * @param translation Translation vector.
   * @param rotation Rotation matrix.
   */
  Transform(Vector translation, Matrix rotation);

  /**
   * @brief Returns the translation component.
   */
  const Vector& Translation() const;

  /**
   * @brief Returns the mutable translation component.
   */
  Vector& Translation();

  /**
   * @brief Sets the translation component.
   */
  void SetTranslation(const Vector& translation);

  /**
   * @brief Returns the rotation component.
   */
  const Matrix& Rotation() const;

  /**
   * @brief Returns the mutable rotation component.
   */
  Matrix& Rotation();

  /**
   * @brief Sets the rotation component.
   */
  void SetRotation(const Matrix& rotation);

  /**
   * @brief Transforms a point from local to parent coordinates.
   */
  Vector Apply(const Vector& point) const;

  /**
   * @brief Transforms a point from parent to local coordinates.
   */
  Vector Inverse(const Vector& point) const;

  /**
   * @brief Returns the inverse transformation.
   */
  Transform Inverse() const;

  /**
   * @brief Composes this transformation with another transformation.
   */
  Transform operator*(const Transform& other) const;

  /**
   * @brief Matrix multiplier.
   */
  static Matrix MatMul(const Matrix& m1, const Matrix& m2);

private:
  Vector m_translation{0.0, 0.0, 0.0};

  Matrix m_rotation{
      1.0, 0.0, 0.0,
      0.0, 1.0, 0.0,
      0.0, 0.0, 1.0
  };
};
/**
 * @brief Coordinate-aware geometric shape.
 *
 * Shape combines an intrinsic AbstractShape representation with a
 * transformation defining its coordinate frame.
 */
class Shape {
public:
  Shape(std::shared_ptr<AbstractShape> geometry,
        Transform transform = {});

  /**
   * @brief Tests whether a point lies inside this shape.
   *
   * The point is expressed in the parent coordinate system and is
   * transformed into the shape's local coordinate system before testing.
   */
  bool Contains(const Transform::Vector& point) const;

  /**
   * @brief Returns the intrinsic shape representation.
   */
  const AbstractShape& Geometry() const;

  /**
   * @brief Returns the transformation of this shape.
   */
  const Transform& GetTransform() const;



  /**
   * @brief Returns the mutable transformation of this shape.
   */
  Transform& GetTransform();

private:
  std::shared_ptr<AbstractShape> m_geometry;
  Transform m_transform;
};

/**
 * @brief Places a geometric object within a parent coordinate system.
 *
 * A Placement associates a shape with a transformation relative to
 * a parent coordinate frame.
 */
class Placement {
public:
  Placement() = default;

  explicit Placement(Transform transform)
  : m_transform(std::move(transform)) {}

  /**
   * @brief Returns the transformation of this placement.
   */
  const Transform& GetTransform() const;

  /**
   * @brief Returns the mutable transformation of this placement.
   */
  Transform& GetTransform();

private:
  Transform m_transform;
};

} // namespace fccvis::geometry
