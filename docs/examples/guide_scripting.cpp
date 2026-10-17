// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// Documentation example: scripting (docs/source/guides/scripting.md).
// C++ scripts of changes, the command interpreter from C++, and a saved
// configuration replayed in a second session.
#include "AdapterRegistry.h"
#include "CameraChanges.h"
#include "fccvis/api/Builders.h"
#include "fccvis/api/Config.h"
#include "fccvis/api/Interpreter.h"
#include "fccvis/stdlib/Filters.h"
#include "fccvis/stdlib/Styles.h"

#include <cstdio>
#include <string>

using namespace fccvis;

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : ".";
  int failures = 0;

  // [script]
  session::Session session("scripting");
  auto csv = adapters::Registry::Global().Create("csv");
  csv->Open(session.Data(), dir + "/hits.csv", {{"entry", "event"}});
  session.DataChanged();

  // A Script is a value: build it once, extend it, run it on any session.
  api::Script views = api::Scene("front").Create()
                          .Layer<data::Point>("hits", "hits")
                          .Where(stdlib::EnergyAbove(0.5))
                          .Sheet(stdlib::HitsByEnergy({0, 3}));
  views.Append(api::Scene("side").Create().Direction({1, 0, 0}).Layer<data::Point>("hits", "hits"));
  views.Append(api::Layout(api::Split({{api::Pane("front"), "Front"}, {api::Pane("side"), "Side"}})));
  const changes::Result done = views.Run(session);  // one batch through Session::Resolve
  // [script]
  failures += done.ok ? 0 : 1;

  // [batch]
  // A batch stops at its first failing change and reports it; the changes
  // before it stay applied.
  api::Script broken = api::Scene("side").Zoom(2);
  broken.Append(api::Scene("front").Create());  // fails: the scene exists
  broken.Append(api::Scene("front").Zoom(3));   // not applied
  const changes::Result r = broken.Run(session);
  std::printf("ok=%d applied=%zu: %s\n", r.ok, r.applied, r.messages.empty() ? "" : r.messages.front().c_str());
  // [batch]
  failures += (r.ok || r.applied != 1) ? 1 : 0;

  // [interpreter]
  // The shell's interpreter works on the same session: commands and C++ mix.
  api::Interpreter shell(session);
  const api::Reply reply = shell.Run(
      "style front/hits when subdetector == \"HCalBarrel\" then marker=square\n"
      "camera side zoom 1.5\n");
  std::printf("%s", reply.Text().c_str());
  // [interpreter]
  failures += reply.ok ? 0 : 1;

  // [config]
  // Replay what was done through the interpreter in a fresh session.
  const std::string json = api::WriteConfig(shell.StateCommands());
  session::Session copy("copy");
  csv->Open(copy.Data(), dir + "/hits.csv", {{"entry", "event"}});
  copy.DataChanged();
  views.Run(copy);
  api::Interpreter replay(copy);
  for (const auto& command : api::ReadConfig(json)) failures += replay.Execute(command).ok ? 0 : 1;
  // [config]

  // [undo]
  // Every batch resolved by the user or the API is one undo step.
  session.Resolve(changes::ChangeCameraMagnification{"side", 4});
  session.Undo();  // back to the previous magnification
  session.Redo();  // and forward again
  // [undo]
  failures += session.FindScene("side")->camera->magnification == 4 ? 0 : 1;

  const bool same = replay.Execute("scene show front").Text() == shell.Execute("scene show front").Text();
  std::printf("replayed %zu commands, same front scene: %s\n", api::ReadConfig(json).size(), same ? "yes" : "no");
  return failures == 0 && same ? 0 : 1;
}
