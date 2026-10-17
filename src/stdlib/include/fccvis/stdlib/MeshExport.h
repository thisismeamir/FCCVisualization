/**
 * @file MeshExport.h
 * @brief Wavefront OBJ/MTL export of triangle meshes (for web viewers such as Phoenix).
 *
 * Objects are written in world coordinates (millimetres), one `o` block per
 * object, with a material per distinct colour in the companion MTL text.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include "Geometry.h"
#include "Style.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace fccvis::stdlib {

/** @brief Collects meshes and writes them as OBJ (+ MTL). */
class ObjWriter {
 public:
  /**
   * @brief Adds a mesh given in world coordinates.
   * @param name      Object name (`o` line); blanks are replaced by '_'.
   * @param vertices  World positions (mm).
   * @param triangles Vertex indices per triangle.
   * @param color     Diffuse colour (alpha becomes the MTL dissolve value), if any.
   */
  void Add(const std::string& name, const std::vector<std::array<double, 3>>& vertices,
           const std::vector<std::array<std::uint32_t, 3>>& triangles, std::optional<style::Color> color = std::nullopt);

  /** @brief Adds a mesh in its local frame, placed by @p transform. */
  void Add(const std::string& name, const geometry::Mesh& mesh, const geometry::Transform& transform,
           std::optional<style::Color> color = std::nullopt);

  /** @brief Number of objects added. */
  [[nodiscard]] std::size_t Objects() const { return m_objects; }

  /**
   * @brief OBJ text.
   * @param mtlFile File name written in the `mtllib` line; empty for none.
   */
  [[nodiscard]] std::string Obj(const std::string& mtlFile = {}) const;

  /** @brief MTL text with one material per colour used. */
  [[nodiscard]] std::string Mtl() const;

 private:
  std::string m_body;
  std::vector<style::Color> m_materials;
  std::size_t m_vertexCount = 0;
  std::size_t m_objects = 0;
};

}  // namespace fccvis::stdlib
