#include "RootScene.h"
#include <TEveManager.h>
#include <TEveScene.h>
#include <TEveViewer.h>
#include <TGLViewer.h>
#include <TGLPerspectiveCamera.h>
#include <TMath.h>
namespace fccvis::backend::root {

RootScene::RootScene(std::shared_ptr<fccvis::scene::Scene> scene)
    : m_scene(std::move(scene))
{
    m_pEveScene = gEve->SpawnNewScene(m_scene->name.c_str());
    m_pViewer = gEve->SpawnNewViewer(m_scene->name.c_str());
    m_pViewer->AddScene(m_pEveScene);

    SyncCamera();
}

RootScene::~RootScene()
{
  // TODO: gEve->GetViewers()->DestroyElement(m_pViewer) / scene cleanup —
  // needs care around TEve's own ownership of these elements.
  // Not sure if this is right...
    gEve->GetViewers()->Destroy();
    gEve->GetScenes()->DestroyScenes();
}
void RootScene::SyncCamera()
{
    if (!m_pViewer || !m_scene || !m_scene->camera) {
        return;
    }

    TGLViewer* glViewer = m_pViewer->GetGLViewer();
    if (!glViewer) {
        return;
    }

    const auto& pos = m_scene->camera->positionVector;
    const auto& dir = m_scene->camera->directionVector;
    if (pos.size() < 3 || dir.size() < 3) {
        return;
    }

    // Normalize direction
    double dx = dir[0], dy = dir[1], dz = dir[2];
    double dlen = std::sqrt(dx*dx + dy*dy + dz*dz);
    if (dlen < 1e-9) {
        return;
    }
    dx /= dlen; dy /= dlen; dz /= dlen;

    // Look-at target: a point ahead of the eye along the direction vector
    Double_t center[3] = {
        pos[0] + dx,
        pos[1] + dy,
        pos[2] + dz
    };

    // dolly: distance from center back to eye (camera sits 'dolly' back from center)
    Double_t dolly = std::sqrt(
        (pos[0]-center[0])*(pos[0]-center[0]) +
        (pos[1]-center[1])*(pos[1]-center[1]) +
        (pos[2]-center[2])*(pos[2]-center[2])
    );

    // hRotate/vRotate express orientation relative to kCameraPerspXOY's
    // default facing axis — calibrate signs/offsets against that convention.
    Double_t hRotate = TMath::RadToDeg() * std::atan2(dy, dx);
    Double_t vRotate = TMath::RadToDeg() * std::asin(dz);

    Double_t fov = 30.0; // expose via SceneOptions if you want it tunable

    glViewer->SetCurrentCamera(TGLViewer::kCameraPerspXOY);
    glViewer->SetPerspectiveCamera(
        TGLViewer::kCameraPerspXOY,
        fov, dolly, center, hRotate, vRotate
    );
    glViewer->RequestDraw();
}
}
