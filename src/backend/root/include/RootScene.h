#include "Scene.h"
#include <TEveManager.h>
#include <TEveScene.h>
#include <TEveViewer.h>
#include <memory>
#include <string>
#include <vector>
namespace fccvis::backend::root {

class RootScene
{
public:
    explicit RootScene(std::shared_ptr<fccvis::scene::Scene> scene);
    ~RootScene();

    RootScene(const RootScene&) = delete;
    RootScene& operator=(const RootScene&) = delete;

    TEveViewer* Viewer() const { return m_pViewer; }
    TEveScene* EveScene() const { return m_pEveScene; }

    void SyncCamera();  // pushes Scene::camera's position/direction into the TGLViewer
private:
    std::shared_ptr<fccvis::scene::Scene> m_scene;
    TEveViewer* m_pViewer;
    TEveScene* m_pEveScene;
};

}
