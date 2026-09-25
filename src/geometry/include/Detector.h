#pragma once

#include "Geometry.h"
#include <DD4hep/DetElement.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>
namespace fccvis::geometry::detector {
class DetectorNode {
public:
  DetectorNode(std::string name, std::string path, Shape shape,
               std::vector<std::shared_ptr<DetectorNode>> children = {})
      : m_name(std::move(name)), m_path(std::move(path)),
        m_shape(std::move(shape)), m_children(std::move(children)) {}

  const std::string &Name() const;
  const std::string &Path() const;
  const Shape &GetShape() const;
  const std::vector<std::shared_ptr<DetectorNode>> &Children() const;

  /// This node's transform composed with every ancestor's, i.e. world frame.
  Transform WorldTransform(const Transform &parentWorld = {}) const;

private:
  std::string m_name;
  std::string m_path;
  Shape m_shape; // local transform lives inside here
  std::vector<std::shared_ptr<DetectorNode>> m_children;
};

std::shared_ptr<DetectorNode> BuildNode(dd4hep::DetElement de);
} // namespace fccvis::geometry::detector
