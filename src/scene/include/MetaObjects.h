#pragma once

#include <memory>
#include <vector>
#include "BaseSessionObject.h"

namespace fccvis::scene::meta {

enum class LayoutKind { Pack, Tab, Leaf };
enum class PackOrientation { Horizontal, Vertical };

struct LayoutNode {
    LayoutKind kind;

    // Pack-only
    PackOrientation orientation = PackOrientation::Horizontal;

    // Leaf-only: name of the Scene this slot materializes
    std::string sceneName;

    // Pack/Tab-only: children, each optionally weighted (Pack) or unweighted (Tab)
    std::vector<std::pair<std::shared_ptr<LayoutNode>, int /*weight, Pack only*/>> children;
};


class Camera : public fccvis::scene::BaseSessionObject
{
public:
    using BaseSessionObject::BaseSessionObject;

    std::vector<double> positionVector;
    std::vector<double> directionVector;
};

class Filter : public BaseSessionObject
{
public:
  using BaseSessionObject::BaseSessionObject;
};

class SceneOptions : public fccvis::scene::BaseSessionObject {
public:
  using BaseSessionObject::BaseSessionObject;
  // TODO: scene options, coloring, and other possible things.
};


}
