// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
#include "Geometry.h"

#include <utility>

namespace fccvis::geometry {

Transform::Vector Transform::Apply(const Transform::Vector &point) const {
  const auto &R = m_rotation;

  return {
      R[0] * point[0] + R[1] * point[1] + R[2] * point[2] + m_translation[0],
      R[3] * point[0] + R[4] * point[1] + R[5] * point[2] + m_translation[1],
      R[6] * point[0] + R[7] * point[1] + R[8] * point[2] + m_translation[2],
  };
}
Transform::Vector Transform::Inverse(const Vector &p) const {
  const Vector d = {p[0] - m_translation[0], p[1] - m_translation[1],
                    p[2] - m_translation[2]};
  const auto &R = m_rotation; // use transpose: R^T * d
  return {
      R[0] * d[0] + R[3] * d[1] + R[6] * d[2],
      R[1] * d[0] + R[4] * d[1] + R[7] * d[2],
      R[2] * d[0] + R[5] * d[1] + R[8] * d[2],
  };
}

Transform Transform::Inverse() const {
  const auto &R = m_rotation;

  Matrix inverseRotation{R[0], R[3], R[6], R[1], R[4], R[7], R[2], R[5], R[8]};

  const Vector inverseTranslation{
      -(R[0] * m_translation[0] + R[3] * m_translation[1] +
        R[6] * m_translation[2]),

      -(R[1] * m_translation[0] + R[4] * m_translation[1] +
        R[7] * m_translation[2]),

      -(R[2] * m_translation[0] + R[5] * m_translation[1] +
        R[8] * m_translation[2])};

  return Transform(inverseTranslation, inverseRotation);
}

Transform::Matrix Transform::MatMul(const Matrix &m1, const Matrix &m2) {
  Transform::Matrix r{};
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      r[3 * i + j] = m1[3 * i] * m2[j] + m1[3 * i + 1] * m2[j + 3] +
                     m1[3 * i + 2] * m2[j + 6];
    }
  }
  return r;
}

Transform Transform::operator*(const Transform &other) const {
  Transform::Matrix R = Transform::MatMul(m_rotation, other.m_rotation);

  Transform::Vector T = {
      m_rotation[0] * other.m_translation[0] +
          m_rotation[1] * other.m_translation[1] +
          m_rotation[2] * other.m_translation[2] + m_translation[0],
      m_rotation[3] * other.m_translation[0] +
          m_rotation[4] * other.m_translation[1] +
          m_rotation[5] * other.m_translation[2] + m_translation[1],
      m_rotation[6] * other.m_translation[0] +
          m_rotation[7] * other.m_translation[1] +
          m_rotation[8] * other.m_translation[2] + m_translation[2],
  };

  return Transform(T, R);
}

bool Shape::Contains(const Transform::Vector &point) const {
  return m_geometry->Criterion(m_transform.Inverse(point)) <= 0.0;
}

Transform::Transform(Vector translation, Matrix rotation)
    : m_translation(translation), m_rotation(rotation) {}

const Transform::Vector &Transform::Translation() const {
  return m_translation;
}
const Transform::Matrix &Transform::Rotation() const { return m_rotation; }

Shape::Shape(std::shared_ptr<AbstractShape> geometry, Transform transform)
    : m_geometry(std::move(geometry)), m_transform(std::move(transform)) {}

const AbstractShape &Shape::Geometry() const { return *m_geometry; }
const Transform &Shape::GetTransform() const { return m_transform; }
Transform &Shape::GetTransform() { return m_transform; }

const Transform &Placement::GetTransform() const { return m_transform; }
Transform &Placement::GetTransform() { return m_transform; }

Transform::Vector &Transform::Translation() { return m_translation; }

void Transform::SetTranslation(const Vector &translation) {
  m_translation = translation;
}

Transform::Matrix &Transform::Rotation() { return m_rotation; }

void Transform::SetRotation(const Matrix &rotation) { m_rotation = rotation; }

} // namespace fccvis::geometry
