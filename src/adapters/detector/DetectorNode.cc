// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
#include "DetectorNode.h"

#include "TGeoShapeAdapter.h"

#include <DD4hep/DetElement.h>
#include <TGeoMatrix.h>

#include <memory>

namespace fccvis::geometry::detector {

const std::string& DetectorNode::Name() const { return m_name; }
const std::string& DetectorNode::Path() const { return m_path; }
const Shape& DetectorNode::GetShape() const { return m_shape; }
const std::vector<std::shared_ptr<DetectorNode>>& DetectorNode::Children() const {
  return m_children;
}

Transform DetectorNode::WorldTransform(const Transform& parentWorld) const {
  return parentWorld * m_shape.GetTransform();
}

std::shared_ptr<DetectorNode> BuildNode(dd4hep::DetElement de) {
  dd4hep::PlacedVolume pv = de.placement();

  TGeoShape* rawShape = pv.volume().solid().ptr();
  std::shared_ptr<const TGeoShape> nonOwning(rawShape, [](const TGeoShape*) {});
  auto adapter = std::make_shared<TGeoShapeAdapter>(std::move(nonOwning));

  Shape shape(adapter, ToTransform(pv.matrix()));

  std::vector<std::shared_ptr<DetectorNode>> children;
  children.reserve(de.children().size());
  for (auto& [childName, childDe] : de.children()) children.push_back(BuildNode(childDe));

  return std::make_shared<DetectorNode>(de.name(), de.path(), std::move(shape),
                                        std::move(children));
}

}  // namespace fccvis::geometry::detector
