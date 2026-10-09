#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <memory>
#include <string>

#include "Changes.h"
#include "Backend.h"
#include "FVBackend.h"
#include "BackendRegistry.h"

using namespace fccvis;

namespace  {

// Mock implementation of Port to record backend output during tests
class DummyPort final : public backend::Port {
 public:
  changes::Result Submit(const changes::Change& change) override {
    submittedChanges.push_back(&change);
    return {};
  }

  void Log(changes::Severity severity, const std::string& message) override {
    lastSeverity = severity;
    lastLog = message;
  }

  void Picked(const backend::PickEvent& event) override {
    lastPick = event;
  }

  void SceneClosed(const std::string& scene) override {
    closedScene = scene;
  }

  void Stopped(const std::string& reason) override {
    stopReason = reason;
  }

  std::vector<const changes::Change*> submittedChanges;
  changes::Severity lastSeverity{};
  std::string lastLog;
  std::optional<backend::PickEvent> lastPick;
  std::optional<std::string> closedScene;
  std::optional<std::string> stopReason;
};

// Mock implementation of SessionView for Sync tests
class DummySessionView final : public backend::SessionView {
 public:
  const data::SessionData& Data() const override {
    return data;
  }

  std::shared_ptr<const scene::Scene> FindScene(
      const std::string& name) const override {
    auto it = scenes.find(name);
    if (it == scenes.end()) {
      return nullptr;
    }
    return it->second;
  }

  changes::SceneRevision Revision(const std::string& name) const override {
    auto it = sceneRevisions.find(name);
    if (it == sceneRevisions.end()) {
      return {};
    }
    return it->second;
  }

  std::uint64_t LayoutRevision() const override {
    return layoutRevision;
  }

  std::uint64_t DataRevision() const override {
    return dataRevision;
  }

  data::SessionData data;

  std::unordered_map<std::string, std::shared_ptr<scene::Scene>> scenes;
  std::unordered_map<std::string, changes::SceneRevision> sceneRevisions;

  std::uint64_t layoutRevision = 1;
  std::uint64_t dataRevision = 1;
};

}  // namespace fccvis::backend::test
TEST_CASE("FVBackend Registration", "[backend][fv]") {
  SECTION("Registered in global registry") {
    auto names = backend::Registry::Global().Names();
    REQUIRE(std::find(names.begin(), names.end(), "fv") != names.end());
  }

  SECTION("Factory creates valid backend instance") {
    auto backend = backend::Registry::Global().Create("fv");
    REQUIRE(backend != nullptr);
    REQUIRE(backend->GetCapabilities().description.find("FV") != std::string::npos);
  }
}

TEST_CASE("FVBackend Lifecycle & Scene Handling", "[backend][fv]") {
  auto backend = backend::Registry::Global().Create("fv");
  REQUIRE(backend != nullptr);

  DummyPort port;
  backend::Options options{{"width", "100"}, {"height", "100"}, {"output", "test_output.ppm"}};

  SECTION("Start and Stop lifecycle") {
    backend->Start("fv_test_id", port, options);
    REQUIRE(port.lastLog.find("100x100") != std::string::npos);

    backend->Stop();
    REQUIRE(port.lastLog == "FVBackend stopped.");
  }

  SECTION("OpenScene validation") {
    DummySessionView view;
    std::string error;

    backend->Start("fv_test_id", port, options);

    // Opening non-existent scene fails
    REQUIRE_FALSE(backend->OpenScene("missing_scene", view, error));
    REQUIRE_FALSE(error.empty());

    // Opening existing scene succeeds
    error.clear();
    REQUIRE(backend->OpenScene("valid_scene", view, error));
    REQUIRE(error.empty());

    backend->Stop();
  }

  SECTION("Sync outputs image file") {
    DummySessionView view;
    backend->Start("fv_test_id", port, options);

    std::string error;
    backend->OpenScene("valid_scene", view, error);

    std::set<std::string> openScenes{"valid_scene"};
    backend::SyncRequest request{view, openScenes, std::nullopt, ""};

    backend->Sync(request);

    // Verify PPM file was written to disk
    std::ifstream file("test_output.ppm");
    REQUIRE(file.is_open());

    std::string header;
    file >> header;
    REQUIRE(header == "P3");

    file.close();
    std::remove("test_output.ppm");

    backend->Stop();
  }
}
