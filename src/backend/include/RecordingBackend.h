#pragma once
/**
 * @file RecordingBackend.h
 * @brief A backend that renders nothing and records everything.
 *
 * Used to test the session and the backend contract without any rendering
 * library, and as a CI backend. Every call is logged; every Sync records which
 * parts and layers were dirty, computed from revisions exactly as a real
 * backend would.
 */

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Backend.h"
#include "BackendRegistry.h"

namespace fccvis::backend {

/** @brief What a recording backend observed; shared with the test that created it. */
struct Recording {
  /** @brief One Sync call. */
  struct SyncRecord {
    std::set<std::string> open;                         ///< Open scenes at the time.
    std::optional<scene::layout::LayoutNode> layout;    ///< Pruned layout received.
    std::string origin;                                 ///< Origin of the triggering change.
    std::map<std::string, changes::Footprint> dirty;    ///< Per open scene: what changed since its last Sync.
    std::uint64_t layoutRevision = 0;
    std::uint64_t dataRevision = 0;
  };

  Port *port = nullptr;                 ///< Set at Start; lets tests inject events.
  std::set<std::string> rejectScenes;   ///< OpenScene fails for these scenes.
  bool throwOnSync = false;             ///< Sync throws, to test failure isolation.

  void Add(std::string call) {
    std::lock_guard<std::mutex> lock(mutex);
    calls.push_back(std::move(call));
  }
  void AddSync(SyncRecord r) {
    std::lock_guard<std::mutex> lock(mutex);
    syncs.push_back(std::move(r));
  }
  [[nodiscard]] std::vector<std::string> Calls() const {
    std::lock_guard<std::mutex> lock(mutex);
    return calls;
  }
  [[nodiscard]] std::size_t SyncCount() const {
    std::lock_guard<std::mutex> lock(mutex);
    return syncs.size();
  }
  /** @brief The most recent Sync, or an empty record if there is none. */
  [[nodiscard]] SyncRecord LastSync() const {
    std::lock_guard<std::mutex> lock(mutex);
    return syncs.empty() ? SyncRecord{} : syncs.back();
  }

 private:
  mutable std::mutex mutex;
  std::vector<std::string> calls;
  std::vector<SyncRecord> syncs;
};

/** @brief Backend that records its calls into a shared @ref Recording. */
class RecordingBackend final : public Backend {
 public:
  explicit RecordingBackend(std::shared_ptr<Recording> rec) : m_rec(std::move(rec)) {}

  [[nodiscard]] Capabilities GetCapabilities() const override {
    Capabilities c;
    c.description = "Records calls; renders nothing.";
    c.kinds = {ElementKind::Point, ElementKind::Line, ElementKind::Surface};
    c.styleProperties = {"color", "opacity", "visible", "size", "marker", "width",
                         "dash", "edgeColor", "edgeWidth", "wireframe",
                         "background", "axes", "grid"};
    return c;
  }

  void Start(const std::string &id, Port &port, const Options &options) override {
    m_id = id;
    m_rec->port = &port;
    m_rec->Add("Start(" + id + ")");
    for (const auto &[k, v] : options) m_rec->Add("Option(" + k + "=" + v + ")");
  }

  void Stop() override {
    m_rec->Add("Stop(" + m_id + ")");
    m_rec->port = nullptr;
  }

  bool OpenScene(const std::string &scene, const SessionView &view, std::string &error) override {
    m_rec->Add("OpenScene(" + scene + ")");
    if (!view.FindScene(scene)) { error = "no such scene"; return false; }
    if (m_rec->rejectScenes.count(scene)) { error = "rejected by test"; return false; }
    return true;
  }

  void CloseScene(const std::string &scene) override {
    m_rec->Add("CloseScene(" + scene + ")");
    m_seen.erase(scene);
  }

  void Sync(const SyncRequest &request) override {
    if (m_rec->throwOnSync) throw std::runtime_error("sync failure (test)");
    Recording::SyncRecord rec;
    rec.open = request.open;
    rec.layout = request.layout;
    rec.origin = request.origin;
    rec.layoutRevision = request.view.LayoutRevision();
    rec.dataRevision = request.view.DataRevision();
    for (const auto &scene : request.open) {
      const auto now = request.view.Revision(scene);
      rec.dirty[scene] = now.Since(m_seen[scene], scene);
      m_seen[scene] = now;
    }
    m_rec->AddSync(std::move(rec));
    m_rec->Add("Sync");
  }

 private:
  std::shared_ptr<Recording> m_rec;
  std::string m_id;
  std::map<std::string, changes::SceneRevision> m_seen;
};

/**
 * @brief Registers a recording backend type under @p name.
 * @return The recording shared by every instance created from that type.
 */
inline std::shared_ptr<Recording> RegisterRecordingBackend(const std::string &name = "recording") {
  auto rec = std::make_shared<Recording>();
  Registry::Global().Register(name, [rec] { return std::make_unique<RecordingBackend>(rec); });
  return rec;
}

}  // namespace fccvis::backend
