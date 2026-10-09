/**
 * @file Session.h
 * @brief Central hub: data, scenes, layout, change resolution and backends.
 *
 * All mutation goes through @ref Session::Resolve, which applies changes,
 * logs them, bumps revisions and notifies the attached backends. Backends are
 * attached with @ref Session::InitBackend, scoped to the scenes opened in
 * them, and talk to the session only through a Port.
 *
 * The session has no knowledge of any input format or rendering target.
 *
 * @note The session holds self-pointers (backend ports), so it is neither
 *       copyable nor movable. Construct it in place or hold it by pointer.
 */
#pragma once

#include "Backend.h"
#include "BackendRegistry.h"
#include "Changes.h"
#include "Data.h"
#include "Layout.h"
#include "Revision.h"
#include "Scene.h"
#include "SceneChanges.h"
#include "SessionOptions.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace fccvis::session {

/** @brief Receives log messages. */
using LogSink = std::function<void(changes::Severity, const std::string &)>;

/** @brief Receives picks reported by backends. */
using PickHandler = std::function<void(const std::string &backendId,
                                       const backend::PickEvent &)>;

class Session {
public:
  /** @brief Constructs an empty session. @param name Session name. */
  explicit Session(std::string name) : m_name(std::move(name)) {}

  /** @brief Stops all attached backends. */
  ~Session() {
    for (auto &[id, att] : m_backends) {
      try {
        att.backend->Stop();
      } catch (...) {
      }
    }
  }

  Session(const Session &) = delete;
  Session &operator=(const Session &) = delete;
  Session(Session &&) = delete;
  Session &operator=(Session &&) = delete;

  /** @brief Session name. */
  const std::string &Name() const noexcept { return m_name; }

  /**
   * @brief Series store. Adapters register series here, then call @ref
   * DataChanged.
   */
  data::SessionData &Data() noexcept { return m_data; }
  const data::SessionData &Data() const noexcept { return m_data; }

  /** @brief Read-only configuration. Modify it through @ref Resolve. */
  const SessionOptions &Options() const noexcept { return m_options; }

  /** @brief Registers a camera in the session's camera list. */
  void AddCamera(fccvis::scene::CameraPtr camera) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_options.cameras.push_back(std::move(camera));
  }

  /** @brief Installs the log sink. Without one, warnings and errors go to
   * stderr. */
  void SetLogSink(LogSink sink) { m_sink = std::move(sink); }

  /** @brief Installs the handler for picks reported by backends. */
  void SetPickHandler(PickHandler handler) {
    m_pickHandler = std::move(handler);
  }

  // ---- change resolution ------------------------------------------------

  /**
   * @brief Executes one change: apply, log, bump revisions, notify backends.
   *
   * @param change Change to execute.
   * @param origin Backend id that caused the change; empty for user or API
   * calls.
   */
  changes::Result Resolve(const changes::Change &change,
                          const std::string &origin = {}) {
    const changes::Change *one[] = {&change};
    return Resolve(std::span<const changes::Change *const>(one), origin);
  }

  /**
   * @brief Executes a batch with one notification at the end.
   *
   * Stops at the first failing change; the result reports how many were
   * applied before it.
   */
  changes::Result Resolve(std::span<const changes::Change *const> batch,
                          const std::string &origin = {}) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    changes::Result result;
    changes::Workspace ws{m_data, m_options.scenes, m_options.sceneLayout};
    const std::string tag = origin.empty() ? "" : "[" + origin + "] ";

    for (const auto *c : batch) {
      try {
        c->Apply(ws);
        Commit(c->Touches(), result);
        ++result.applied;
        Log(changes::Severity::Info, tag + c->Describe());
      } catch (const std::exception &e) {
        result.ok = false;
        result.messages.push_back(c->Name() + ": " + e.what());
        Log(changes::Severity::Error, tag + c->Name() + " failed: " + e.what());
        break;
      }
    }
    if (result.applied > 0)
      SyncAll(origin, result);
    return result;
  }

  /** @brief Tells the backends that series were registered or changed. */
  void DataChanged() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_dataRevision = ++m_clock;
    changes::Result ignored;
    SyncAll({}, ignored);
  }

  // ---- scenes and layout -------------------------------------------------

  /** @brief Names of all scenes, sorted. */
  std::vector<std::string> SceneNames() const {
    std::vector<std::string> names;
    names.reserve(m_options.scenes.size());
    for (const auto &[name, _] : m_options.scenes)
      names.push_back(name);
    std::sort(names.begin(), names.end());
    return names;
  }

  /** @brief Creates a scene through @ref Resolve. @return The scene, or nullptr
   * on failure. */
  std::shared_ptr<fccvis::scene::Scene> CreateScene(const std::string &name) {
    if (!Resolve(changes::CreateScene{name}).ok)
      return nullptr;
    return FindScene(name);
  }

  /** @brief Looks up a scene. @return The scene, or nullptr. */
  std::shared_ptr<fccvis::scene::Scene>
  FindScene(const std::string &name) const {
    auto it = m_options.scenes.find(name);
    return it == m_options.scenes.end() ? nullptr : it->second;
  }

  /** @brief True if a scene of this name exists. */
  bool HasScene(const std::string &name) const {
    return m_options.scenes.find(name) != m_options.scenes.end();
  }

  /** @brief Sets the layout through @ref Resolve. */
  changes::Result SetLayout(fccvis::scene::layout::LayoutNode layout) {
    return Resolve(changes::SetLayout{std::move(layout)});
  }

  // ---- backends ------------------------------------------------------------

  /**
   * @brief Creates a backend of a registered type and attaches it.
   *
   * @param type    Registered backend name, for example "root".
   * @param id      Instance id; defaults to the type name. Must be unique.
   * @param options Per-instance options.
   */
  changes::Result InitBackend(const std::string &type, std::string id = {},
                              const backend::Options &options = {}) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    changes::Result r;
    if (id.empty())
      id = type;
    auto existing = m_backends.find(id);
    if (existing != m_backends.end()) {
      if (!existing->second.stopped)
        return Fail(r, "backend id '" + id + "' is already in use");
      try {
        existing->second.backend->Stop();
      } catch (...) {
      }
      m_backends.erase(existing);
    }

    auto b = backend::Registry::Global().Create(type);
    if (!b) {
      std::string known;
      for (const auto &n : backend::Registry::Global().Names())
        known += " " + n;
      return Fail(r, "unknown backend '" + type + "'; available:" + known);
    }

    Attached att;
    att.port = std::make_unique<PortImpl>(*this, id);
    try {
      b->Start(id, *att.port, options);
    } catch (const std::exception &e) {
      return Fail(r, "backend '" + id + "' failed to start: " + e.what());
    }
    att.backend = std::move(b);
    m_backends.emplace(id, std::move(att));
    Log(changes::Severity::Info,
        "backend '" + id + "' attached (" + type + ")");
    return r;
  }

  /** @brief Closes all scenes of a backend, stops it and detaches it. */
  changes::Result DetachBackend(const std::string &id) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    changes::Result r;
    auto it = m_backends.find(id);
    if (it == m_backends.end())
      return Fail(r, "unknown backend '" + id + "'");
    for (const auto &scene : it->second.open) {
      try {
        it->second.backend->CloseScene(scene);
      } catch (...) {
      }
    }
    try {
      it->second.backend->Stop();
    } catch (...) {
    }
    m_backends.erase(it);
    Log(changes::Severity::Info, "backend '" + id + "' detached");
    return r;
  }

  /** @brief Opens a scene in a backend and draws it. */
  changes::Result Open(const std::string &id, const std::string &scene) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    changes::Result r;
    auto it = m_backends.find(id);
    if (it->second.stopped)
      return Fail(r, "backend '" + id + "' has stopped.");
    if (it == m_backends.end())
      return Fail(r, "unknown backend '" + id + "'");
    if (!HasScene(scene))
      return Fail(r, "unknown scene '" + scene + "'");
    if (it->second.open.count(scene))
      return r;

    std::string error;
    View view(*this);
    try {
      if (!it->second.backend->OpenScene(scene, view, error))
        return Fail(r, "backend '" + id + "' cannot open '" + scene +
                           "': " + error);
    } catch (const std::exception &e) {
      return Fail(r, "backend '" + id + "' failed opening '" + scene +
                         "': " + e.what());
    }
    it->second.open.insert(scene);
    SyncOne(id, it->second, {}, r);
    return r;
  }

  /** @brief Closes a scene in a backend. The scene and its state stay in the
   * session. */
  changes::Result Close(const std::string &id, const std::string &scene) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    changes::Result r;
    auto it = m_backends.find(id);
    if (it->second.stopped)
      return r;
    if (it == m_backends.end())
      return Fail(r, "unknown backend '" + id + "'");
    if (!it->second.open.erase(scene))
      return r;
    try {
      it->second.backend->CloseScene(scene);
    } catch (...) {
    }
    SyncOne(id, it->second, {}, r);
    return r;
  }

  /** @brief Ids of the attached backends, sorted. */
  std::vector<std::string> BackendIds() const {
    std::vector<std::string> ids;
    for (const auto &[id, att] : m_backends)
      ids.push_back(id);
    return ids;
  }

  /** @brief Scenes open in a backend. */
  std::set<std::string> OpenScenes(const std::string &id) const {
    auto it = m_backends.find(id);
    return it == m_backends.end() ? std::set<std::string>{} : it->second.open;
  }

  // ---- validation ----------------------------------------------------------

  /** @brief Checks the layout against the registered scenes. */
  std::vector<std::string> ValidateLayout() const {
    if (!m_options.sceneLayout)
      return {};
    return fccvis::scene::layout::Validate(
        *m_options.sceneLayout,
        [this](const std::string &name) { return HasScene(name); });
  }

  /** @brief Checks every scene's layers against the data. */
  std::vector<std::string> ValidateScenes() const {
    std::vector<std::string> errors;
    for (const auto &name : SceneNames()) {
      const auto scene = FindScene(name);
      if (!scene)
        continue;
      for (auto &e : scene->Validate(m_data))
        errors.push_back("scene '" + name + "': " + e);
    }
    return errors;
  }

  /** @brief Layout and scene validation together. */
  std::vector<std::string> Validate() const {
    auto errors = ValidateLayout();
    for (auto &e : ValidateScenes())
      errors.push_back(std::move(e));
    return errors;
  }
  /** @brief True if the backend is attached and has not stopped. */
  bool IsRunning(const std::string &id) const {
    auto it = m_backends.find(id);
    return it != m_backends.end() && !it->second.stopped;
  }

private:
  class PortImpl final : public backend::Port {
  public:
    PortImpl(Session &s, std::string id) : m_s(s), m_id(std::move(id)) {}
    changes::Result Submit(const changes::Change &c) override {
      return m_s.Resolve(c, m_id);
    }
    void Log(changes::Severity sev, const std::string &msg) override {
      m_s.Log(sev, m_id + ": " + msg);
    }
    void Picked(const backend::PickEvent &e) override {
      if (m_s.m_pickHandler)
        m_s.m_pickHandler(m_id, e);
    }
    void SceneClosed(const std::string &scene) override {
      std::lock_guard<std::recursive_mutex> lock(m_s.m_mutex);
      auto it = m_s.m_backends.find(m_id);
      if (it != m_s.m_backends.end())
        it->second.open.erase(scene);
    }
    void Stopped(const std::string &reason) override {
      std::lock_guard<std::recursive_mutex> lock(m_s.m_mutex);
      auto it = m_s.m_backends.find(m_id);
      if (it == m_s.m_backends.end() || it->second.stopped)
        return;
      it->second.stopped = true;
      it->second.open.clear();
      m_s.Log(changes::Severity::Warning,
              "backend '" + m_id + "' stopped: " + reason);
    }

  private:
    Session &m_s;
    std::string m_id;
  };

  class View final : public backend::SessionView {
  public:
    explicit View(const Session &s) : m_s(s) {}
    const data::SessionData &Data() const override { return m_s.m_data; }
    std::shared_ptr<const fccvis::scene::Scene>
    FindScene(const std::string &n) const override {
      return m_s.FindScene(n);
    }
    changes::SceneRevision Revision(const std::string &n) const override {
      auto it = m_s.m_revisions.find(n);
      return it == m_s.m_revisions.end() ? changes::SceneRevision{}
                                         : it->second;
    }
    std::uint64_t LayoutRevision() const override {
      return m_s.m_layoutRevision;
    }
    std::uint64_t DataRevision() const override { return m_s.m_dataRevision; }

  private:
    const Session &m_s;
  };

  struct Attached {
    std::unique_ptr<backend::Backend> backend;
    std::unique_ptr<PortImpl> port;
    std::set<std::string> open;
    bool stopped = false;
  };

  changes::Result &Fail(changes::Result &r, const std::string &message) {
    r.ok = false;
    r.messages.push_back(message);
    Log(changes::Severity::Error, message);
    return r;
  }

  void Log(changes::Severity sev, const std::string &msg) const {
    if (m_sink)
      m_sink(sev, msg);
    else if (sev != changes::Severity::Info)
      std::cerr << msg << '\n';
  }

  // Bumps revisions for the parts a change touched.
  void Commit(const changes::Footprint &f, changes::Result &result) {
    using changes::Has;
    using changes::Part;
    const std::uint64_t tick = ++m_clock;
    const auto scene = FindScene(f.scene);

    if (Has(f.parts, Part::Layout))
      m_layoutRevision = tick;
    if (Has(f.parts, Part::Data))
      m_dataRevision = tick;

    if (!f.scene.empty()) {
      auto &rev = m_revisions[f.scene];
      if (Has(f.parts, Part::Scenes)) { // new scene: everything is new
        rev.camera = rev.cameraOptions = rev.environment = rev.layerSet = tick;
      }
      if (Has(f.parts, Part::Camera) && scene && scene->camera) {
        for (const auto &[name, other] : m_options.scenes) {
          if (other && other->camera == scene->camera)
            m_revisions[name].camera = tick;
        }
      }
      if (Has(f.parts, Part::CameraOptions))
        rev.cameraOptions = tick;
      if (Has(f.parts, Part::Environment))
        rev.environment = tick;

      if (Has(f.parts, Part::LayerSet) && scene) {
        rev.layerSet = tick;
        const auto names = scene->layers.Names();
        for (auto it = rev.layer.begin(); it != rev.layer.end();) {
          if (std::find(names.begin(), names.end(), it->first) == names.end())
            it = rev.layer.erase(it);
          else
            ++it;
        }
        for (const auto &n : names)
          rev.layer.try_emplace(n, tick);
      }
      if (f.allLayers && scene)
        for (const auto &n : scene->layers.Names())
          rev.layer[n] = tick;
      for (const auto &l : f.layers)
        rev.layer[l] = tick;
    }
    result.touched.push_back(f);
  }

  void SyncOne(const std::string &id, Attached &att, const std::string &origin,
               changes::Result &result) {
    if (att.stopped)
      return;
    try {
      View view(*this);
      auto layout = PrunedLayout(att.open);
      att.backend->Sync(backend::SyncRequest{view, att.open, layout, origin});
    } catch (const std::exception &e) {
      const std::string msg = "backend '" + id + "' sync failed: " + e.what();
      result.messages.push_back(msg);
      Log(changes::Severity::Error, msg);
    }
  }

  void SyncAll(const std::string &origin, changes::Result &result) {
    for (auto &[id, att] : m_backends)
      SyncOne(id, att, origin, result);
  }

  std::optional<fccvis::scene::layout::LayoutNode>
  PrunedLayout(const std::set<std::string> &open) const {
    if (!m_options.sceneLayout)
      return std::nullopt;
    return fccvis::scene::layout::Prune(
        *m_options.sceneLayout,
        [&open](const std::string &name) { return open.count(name) > 0; });
  }

  std::string m_name;
  data::SessionData m_data;
  SessionOptions m_options;

  std::map<std::string, changes::SceneRevision> m_revisions;
  std::uint64_t m_clock = 0;
  std::uint64_t m_layoutRevision = 0;
  std::uint64_t m_dataRevision = 0;

  std::map<std::string, Attached> m_backends;
  LogSink m_sink;
  PickHandler m_pickHandler;
  mutable std::recursive_mutex m_mutex;
};

} // namespace fccvis::session
