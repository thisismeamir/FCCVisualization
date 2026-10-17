// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// Experiment 1/5/6: classic TEve under a display, event loop driven by ProcessEvents,
// two "instances", offscreen picture.
#include <TApplication.h>
#include <TEveManager.h>
#include <TEveViewer.h>
#include <TEvePointSet.h>
#include <TEveBrowser.h>
#include <TGLViewer.h>
#include <TSystem.h>
#include <TROOT.h>
#include <cstdio>
#include <thread>
#include <atomic>

int main(int argc, char** argv) {
  TApplication app("exp", &argc, argv);
  std::printf("batch=%d display=%s\n", gROOT->IsBatch(), getenv("DISPLAY") ? getenv("DISPLAY") : "(none)");
  TEveManager* m = TEveManager::Create(kTRUE, "V");
  std::printf("create -> %p gEve=%p\n", (void*)m, (void*)gEve);
  if (!m) return 1;
  TEveManager* m2 = TEveManager::Create(kTRUE, "V");
  std::printf("second create -> %p (same=%d)\n", (void*)m2, m2 == m);
  auto* ps = new TEvePointSet("pts");
  for (int i = 0; i < 100; ++i) ps->SetNextPoint(i, i % 10, 0);
  ps->SetMarkerColor(kRed);
  gEve->AddElement(ps);
  gEve->Redraw3D(kTRUE);
  for (int i = 0; i < 50; ++i) { gSystem->ProcessEvents(); gSystem->Sleep(20); }
  TGLViewer* v = gEve->GetDefaultGLViewer();
  std::printf("glviewer=%p\n", (void*)v);
  if (v) {
    bool ok1 = v->SavePicture("teve_save.png");
    bool ok2 = v->SavePictureUsingFBO("teve_fbo.png", 400, 300);
    std::printf("SavePicture=%d SavePictureUsingFBO=%d\n", ok1, ok2);
  }
  std::printf("done\n");
  return 0;
}
