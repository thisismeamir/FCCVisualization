#pragma once
#include "RootScene.h"
#include "Scene.h"
#include "Session.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace fccvis::backend::root {

class RootSceneManager
{
public:
    explicit RootSceneManager(fccvis::session::Session& session);

    // Materializes abstract Scene 'sceneName' into a live TEveScene/TEveViewer.
    // Returns the existing RootScene if already open.
    RootScene* Open(const std::string& sceneName);

    // Tears down this scene's TEve-side objects only. Abstract Scene/data untouched.
    void Close(const std::string& sceneName);

    // Re-pushes camera state into an already-open scene.
    // Filters/data are shared_ptrs into the same objects TEve already draws
    // from, so those stay live without any action here — this only covers
    // what SyncCamera() explicitly pushes (camera position/direction).
    bool Update(const std::string& sceneName);

    RootScene* Get(const std::string& sceneName) const;
    bool IsOpen(const std::string& sceneName) const;
    void CloseAll();
    std::vector<std::shared_ptr<fccvis::scene::Scene>> OpenScenes();
    std::vector<std::shared_ptr<std::string>> SceneNames();
private:
    fccvis::session::Session& m_session;
    std::map<std::string, std::unique_ptr<RootScene>> m_openScenes;

    std::shared_ptr<fccvis::scene::Scene> FindAbstractScene(const std::string& sceneName) const;
};

} // namespace fccvis::backend::root
