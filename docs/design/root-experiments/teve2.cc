// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// Experiments 3/4 with classic TEve: picks via selection polling, closing the browser.
#include <TApplication.h>
#include <TEveManager.h>
#include <TEveSelection.h>
#include <TEvePointSet.h>
#include <TEveBrowser.h>
#include <TGLViewer.h>
#include <TGLCamera.h>
#include <TSystem.h>
#include <cstdio>

int main(int argc, char** argv) {
  TApplication app("exp", &argc, argv);
  TEveManager::Create(kTRUE, "V");
  auto* ps = new TEvePointSet("pts");
  for (int i = 0; i < 10; ++i) ps->SetNextPoint(i, 0, 0);
  gEve->AddElement(ps);
  gEve->Redraw3D(kTRUE);
  for (int i = 0; i < 20; ++i) gSystem->ProcessEvents();
  // picks: a user pick goes into gEve->GetSelection(); poll it
  gEve->GetSelection()->UserPickedElement(ps);
  int n = 0;
  for (auto it = gEve->GetSelection()->BeginChildren(); it != gEve->GetSelection()->EndChildren(); ++it) {
    std::printf("selected: %s\n", (*it)->GetElementName());
    ++n;
  }
  std::printf("selection size %d\n", n);
  // camera: no signal; poll the camera matrix
  TGLCamera& cam = gEve->GetDefaultGLViewer()->CurrentCamera();
  TGLMatrix before = cam.GetCamTrans();
  cam.RotateRad(0.3, 0.2);
  gEve->GetDefaultGLViewer()->RequestDraw();
  for (int i = 0; i < 10; ++i) gSystem->ProcessEvents();
  TGLMatrix after = cam.GetCamTrans();
  bool moved = false;
  for (int i = 0; i < 16; ++i) moved |= before.CArr()[i] != after.CArr()[i];
  std::printf("camera moved detectable by polling: %d\n", moved);
  // close the main window as the window manager would
  gEve->GetBrowser()->CloseWindow();
  for (int i = 0; i < 10; ++i) gSystem->ProcessEvents();
  std::printf("after CloseWindow: gEve=%p (process still running)\n", (void*)gEve);
  return 0;
}
