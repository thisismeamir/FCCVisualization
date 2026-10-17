// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// Documentation example: writing a backend (docs/source/development/index.md).
// A file backend that writes, per open scene, how many elements each layer
// draws: the smallest useful FileBackend.
#include "BackendRegistry.h"
#include "FileBackend.h"
#include "SceneChanges.h"
#include "Session.h"

#include <cstdio>
#include <fstream>
#include <sstream>

using namespace fccvis;

// [backend]
namespace {

class SummaryBackend final : public backend::FileBackend {
 public:
  [[nodiscard]] backend::Capabilities GetCapabilities() const override {
    backend::Capabilities c;
    c.description = "text summary: elements drawn per layer";
    c.kinds = {backend::ElementKind::Point, backend::ElementKind::Line, backend::ElementKind::Surface};
    c.styleProperties = {"visible"};  // the only style property it honours
    c.splits = false;
    c.tabs = false;
    c.options = {{"output_dir", "directory of the <scene>.txt files"}};
    return c;
  }

 protected:
  [[nodiscard]] std::string Kind() const override { return "summary"; }

  // Called on the backend's own thread after every sync that changed something;
  // `job` holds the drawings of the open scenes (styles resolved, filters applied).
  void Render(const Job& job) override {
    for (const auto& [name, scene] : job.scenes) {
      std::ofstream out(ScenePath(name, ".txt"));
      for (const auto& [layer, drawing] : scene.layers) {
        if (!drawing) continue;
        std::size_t visible = 0;
        for (const auto& p : drawing->points) visible += backend::draw::Visible(p.s.common) ? 1 : 0;
        out << layer << ": " << visible << " points, " << drawing->lines.size() << " lines, "
            << drawing->meshes.size() << " surfaces\n";
      }
    }
  }
};

const backend::Registrar g_summary("summary", [] { return std::make_unique<SummaryBackend>(); });

}  // namespace
// [backend]

int main() {
  session::Session session("extend");
  std::vector<data::PointElement> hits(5);
  for (std::size_t i = 0; i < hits.size(); ++i) hits[i].meta.Set("energy", static_cast<double>(i));
  session.Data().Add<data::PointElement>("hits", "example",
      std::make_unique<data::InMemorySeries<data::PointElement>>(std::vector<std::vector<data::PointElement>>{hits}));
  session.DataChanged();
  session.CreateScene("s");
  session.Resolve(changes::AddLayer<data::PointElement>{"s", "hits", {"hits", ""}});

  // [use]
  session.InitBackend("summary", "txt", {{"output_dir", "summary-out"}});
  session.Open("txt", "s");
  session.Flush("txt");  // summary-out/s.txt: "hits: 5 points, 0 lines, 0 surfaces"
  // [use]

  std::ifstream in("summary-out/s.txt");
  std::stringstream text;
  text << in.rdbuf();
  std::printf("%s", text.str().c_str());
  return text.str() == "hits: 5 points, 0 lines, 0 surfaces\n" ? 0 : 1;
}
