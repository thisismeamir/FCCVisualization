// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// Experiment 2: REve without DISPLAY: server mode, URL, HTTP reachable.
#include <ROOT/REveManager.hxx>
#include <ROOT/REvePointSet.hxx>
#include <ROOT/REveScene.hxx>
#include <ROOT/RWebWindow.hxx>
#include <ROOT/RWebDisplayArgs.hxx>
#include <TApplication.h>
#include <TSystem.h>
#include <cstdio>
#include <cstdlib>
namespace REX = ROOT::Experimental;
int main(int argc, char** argv) {
  TApplication app("exp", &argc, argv);
  auto* eve = REX::REveManager::Create();
  auto* ps = new REX::REvePointSet("pts");
  for (int i = 0; i < 10; ++i) ps->SetNextPoint(i, 0, 0);
  eve->GetEventScene()->AddElement(ps);
  ROOT::RWebDisplayArgs args("server");
  eve->Show(args);
  auto win = eve->GetWebWindow();
  std::printf("url=%s\n", win ? win->GetUrl(false).c_str() : "(none)");
  std::fflush(stdout);
  for (int i = 0; i < 300; ++i) { gSystem->ProcessEvents(); gSystem->Sleep(10); }
  std::printf("done\n");
  std::_Exit(0);
}
