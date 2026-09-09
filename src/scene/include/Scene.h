#pragma once
#include "BaseSessionObject.h"
#include "MetaObjects.h"
#include <utility>
namespace fccvis::scene {
class Scene : public BaseSessionObject {
public:
  Scene(std::string sceneName, std::string optionsName)
      : BaseSessionObject(std::move(sceneName)), options(std::move(optionsName)) {}

  std::shared_ptr<fccvis::scene::meta::Camera> camera;
  std::vector<std::shared_ptr<fccvis::scene::meta::Filter>> filters;
  fccvis::scene::meta::SceneOptions options;
};


} // namespace fccvis::scene
