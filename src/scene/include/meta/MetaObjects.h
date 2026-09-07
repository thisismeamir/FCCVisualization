#pragma once

#include <memory>
#include <vector>
#include "BaseSessionObject.h"
namespace fccvis::scene::meta {
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
  // TODO: filter payload — geometric region, detector-component constraint, etc.
};

class SceneOptions : public fccvis::scene::BaseSessionObject {
public:
  using BaseSessionObject::BaseSessionObject;
  // TODO: scene options, coloring, and other possible things.
};


}
