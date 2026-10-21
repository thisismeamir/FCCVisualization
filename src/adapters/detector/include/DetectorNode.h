/**
 * @file DetectorNode.h
 * @brief Tree of detector elements with their shapes, built from DD4hep.
 *
 * Part of the detector adapter (A1); only available when the build enables
 * `FCCVIS_ENABLE_DD4HEP`. Lengths are millimetres (see TGeoShapeAdapter.h).
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include "Geometry.h"

#include <DD4hep/DetElement.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace fccvis::geometry::detector {

/** @brief One detector element: name, DD4hep path, placed shape and children. */
class DetectorNode {
 public:
  /**
   * @param name     Element name.
   * @param path     DD4hep path of the element.
   * @param shape    Shape with the element's local placement.
   * @param children Child elements.
   */
  DetectorNode(std::string name, std::string path, Shape shape,
               std::vector<std::shared_ptr<DetectorNode>> children = {})
      : m_name(std::move(name)), m_path(std::move(path)),
        m_shape(std::move(shape)), m_children(std::move(children)) {}

  /** @brief Element name. */
  const std::string& Name() const;
  /** @brief DD4hep path. */
  const std::string& Path() const;
  /** @brief Shape and local transform. */
  const Shape& GetShape() const;
  /** @brief Child elements. */
  const std::vector<std::shared_ptr<DetectorNode>>& Children() const;

  /**
   * @brief This node's transform composed with its ancestors', i.e. the world frame.
   * @param parentWorld World transform of the parent.
   */
  Transform WorldTransform(const Transform& parentWorld = {}) const;

 private:
  std::string m_name;
  std::string m_path;
  Shape m_shape;  // local transform lives inside here
  std::vector<std::shared_ptr<DetectorNode>> m_children;
};

/**
 * @brief Builds the node tree of a DD4hep element recursively.
 * @param de Element to convert; its solid is wrapped, not copied.
 */
std::shared_ptr<DetectorNode> BuildNode(dd4hep::DetElement de);

}  // namespace fccvis::geometry::detector
