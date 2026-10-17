// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// Experiment 3/4 for REve with a headless browser: picks, camera, close.
#include <ROOT/REveManager.hxx>
#include <ROOT/REvePointSet.hxx>
#include <ROOT/REveScene.hxx>
#include <ROOT/REveSelection.hxx>
#include <ROOT/REveViewer.hxx>
#include <ROOT/RWebWindow.hxx>
#include <ROOT/RWebDisplayArgs.hxx>
#include <TApplication.h>
#include <TSystem.h>
#include <cstdio>
#include <fstream>
namespace REX = ROOT::Experimental;
struct Dev : REX::REveSelection::Deviator {
  bool DeviateSelection(REX::REveSelection* s, REX::REveElement* el, bool multi, bool secondary,
                        const std::set<int>& idx) override {
    std::printf("PICK %s secondary=%d n=%zu first=%d\n", el ? el->GetCName() : "(null)", secondary, idx.size(),
                idx.empty() ? -1 : *idx.begin());
    std::fflush(stdout);
    ExecuteNewElementPicked(s, el, multi, secondary, idx);
    return true;
  }
};
int main(int argc, char** argv) {
  TApplication app("exp", &argc, argv);
  auto* eve = REX::REveManager::Create();
  auto* ps = new REX::REvePointSet("pts");
  for (int i = -20; i <= 20; ++i) for (int j = -20; j <= 20; ++j) ps->SetNextPoint(i * 5, j * 5, 0);
  ps->SetMarkerSize(20);
  ps->SetMarkerColor(kRed);
  ps->SetAlwaysSecSelect(true);
  eve->GetEventScene()->AddElement(ps);
  eve->GetSelection()->SetDeviator(std::make_shared<Dev>());
  ROOT::RWebDisplayArgs args("server");
  eve->Show(args);
  auto win = eve->GetWebWindow();
  std::ofstream("url.txt") << win->GetUrl(false);
  std::printf("url=%s\n", win->GetUrl(false).c_str()); std::fflush(stdout);
  int last = -1;
  for (int i = 0; i < 6000; ++i) {
    gSystem->ProcessEvents(); gSystem->Sleep(10);
    int n = win->NumConnections();
    if (n != last) { std::printf("CONN %d\n", n); std::fflush(stdout); last = n; }
    if (i > 500 && n == 0 && last == 0 && std::ifstream("stop").good()) break;
  }
  std::printf("done\n");
  std::_Exit(0);
}
