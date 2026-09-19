#include "RootSceneManager.h"
#include "Scene.h"
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <CollectionVisualizer.h>

namespace fccvis::backend::root {

RootSceneManager::RootSceneManager(fccvis::session::Session &session)
    : m_session(session) {}

std::shared_ptr<fccvis::scene::Scene>
RootSceneManager::FindAbstractScene(const std::string &sceneName) const {
  for (const auto &scene : m_session.GetOptions().scenes) {
    if (scene->name == sceneName) {
      return scene;
    }
  }
  return nullptr;
}

RootScene *RootSceneManager::Open(const std::string &sceneName) {
  if (auto it = m_openScenes.find(sceneName); it != m_openScenes.end()) {
    return it->second.get();
  }

  auto abstractScene = FindAbstractScene(sceneName);
  if (!abstractScene) {
    std::cerr << "fccvis: no scene named '" << sceneName << "'\n";
    return nullptr;
  }

  auto rootScene = std::make_unique<RootScene>(abstractScene);
  RootScene *raw = rootScene.get();
  m_openScenes.emplace(sceneName, std::move(rootScene));
  return raw;
}

void RootSceneManager::Close(const std::string &sceneName) {
  m_openScenes.erase(
      sceneName); // RootScene's destructor tears down only its own TEve objects
}

bool RootSceneManager::Update(const std::string &sceneName) {
  auto it = m_openScenes.find(sceneName);
  if (it == m_openScenes.end()) {
    return false;
  }
  it->second->SyncCamera();
  return true;
}

RootScene *RootSceneManager::Get(const std::string &sceneName) const {
  auto it = m_openScenes.find(sceneName);
  return it != m_openScenes.end() ? it->second.get() : nullptr;
}

bool RootSceneManager::IsOpen(const std::string &sceneName) const {
  return m_openScenes.count(sceneName) > 0;
}

void RootSceneManager::CloseAll() {
  m_openScenes.clear(); // each RootScene destructs independently, in
                        // map-iteration order
}

std::vector<std::shared_ptr<fccvis::scene::Scene>> RootSceneManager::OpenScenes() {
  std::vector<std::shared_ptr<fccvis::scene::Scene>> abstractScenes;
  for (const auto& rootScene: m_openScenes) {
    abstractScenes.push_back(rootScene.second->GetSceneAbstract());
  }
  return abstractScenes;
}

std::vector<std::shared_ptr<std::string>> RootSceneManager::SceneNames() {
  std::vector<std::shared_ptr<std::string>> names{};

  for (const auto &scene : m_session.GetOptions().scenes) {
    names.push_back(std::make_shared<std::string>(scene->name));
  }

  return names;
}

} // namespace fccvis::backend::root
