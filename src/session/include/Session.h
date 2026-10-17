/**
 * @file Session.h
 * @brief Central hub: data, scenes, layout, change resolution and backends.
 *
 * All mutation goes through fccvis::session::Session::Resolve, which applies changes,
 * logs them, bumps revisions and notifies the attached backends. Backends are
 * attached with fccvis::session::Session::InitBackend, scoped to the scenes opened in
 * them, and talk to the session only through a Port.
 *
 * The session has no knowledge of any input format or rendering target.
 *
 * @note The session holds self-pointers (backend ports), so it is neither
 *       copyable nor movable. Construct it in place or hold it by pointer.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
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
#include <optional>
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

/**
 * @brief A visualization session: series, scenes, layout and attached backends.
 *
 * Thread-safe: every public member may be called from any thread (one
 * recursive mutex). Mutation only through Resolve.
 */
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
  /** @brief The series store (read-only). */
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
    changes::Workspace ws{m_data, m_options.scenes, m_options.sceneLayout, origin};
    const std::string tag = origin.empty() ? "" : "[" + origin + "] ";

    // Undo history: only changes from the user/API (no origin) are recorded;
    // a change without an inverse clears the history.
    const bool record = origin.empty();
    std::vector<std::shared_ptr<const changes::Change>> inverses;
    bool invertible = true;
    for (const auto *c : batch) {
      try {
        if (record && invertible) {
          auto inverse = c->Inverse(ws);
          if (inverse) inverses.push_back(std::move(inverse));
          else invertible = false;
        }
        c->Apply(ws);
        Commit(c->Touches(), result);
        ++result.applied;
        Log(changes::Severity::Info, tag + c->Describe());
      } catch (const std::exception &e) {
        result.ok = false;
        result.messages.push_back(c->Name() + ": " + e.what());
        Log(changes::Severity::Error, tag + c->Name() + " failed: " + e.what());
        // A backend already shows its rejected camera move; make it redraw the
        // session's camera so its view and the session agree again.
        const auto f = c->Touches();
        if (!origin.empty() && changes::Has(f.parts, changes::Part::Camera))
          ResyncCamera(origin, f.scene);
        break;
      }
    }
    if (result.applied > 0) {
      for (const auto &f : result.touched)
        if (changes::Has(f.parts, changes::Part::Scenes) && !f.scene.empty() &&
            !HasScene(f.scene))
          Forget(f.scene);
      for (const auto &f : result.touched)
        if (changes::Has(f.parts, changes::Part::LayerSet))
          for (auto &[id, att] : m_backends)
            if (att.open.count(f.scene))
              CheckCapabilities(id, att, f.scene);
      SyncAll(origin, result);
    }
    if (record) {
      if (!invertible) {
        ClearUndo();
      } else if (result.applied > 0) {
        inverses.resize(std::min(inverses.size(), result.applied));  // the failing change was not applied
        std::reverse(inverses.begin(), inverses.end());
        RecordUndo(std::move(inverses), batch.size() == 1 ? batch.front()->Describe()
                                                          : std::to_string(result.applied) + " changes");
      }
    }
    return result;
  }

  // ---- undo / redo --------------------------------------------------------

  /**
   * @brief Starts grouping: every change resolved until the matching
   *        EndUndoGroup() is undone in one step, described by @p label
   *        (the shell groups each command). Groups nest; the outermost wins.
   */
  void BeginUndoGroup(const std::string &label) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_undoGroupDepth++ == 0) {
      m_undoGroup = UndoStep{{}, label};
      m_undoGroupUsed = false;
      m_undoGroupBroken = false;
    }
  }

  /** @brief Ends the group started by BeginUndoGroup(). */
  void EndUndoGroup() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_undoGroupDepth == 0 || --m_undoGroupDepth > 0) return;
    if (m_undoGroupUsed && !m_undoGroup.changes.empty()) PushUndo(std::move(m_undoGroup), m_replay == ReplayMode::None);
    m_undoGroup = {};
  }

  /**
   * @brief Undoes the last recorded step (a batch, or a group) by resolving
   *        its inverse changes, and makes it available to Redo().
   * @return The result of resolving the inverses; not ok with a message when
   *         there is nothing to undo.
   */
  changes::Result Undo() { return DoReplay(ReplayMode::Undo); }

  /** @brief Re-applies the last undone step. */
  changes::Result Redo() { return DoReplay(ReplayMode::Redo); }

  /** @brief Increases every time a new undo step is recorded (front ends use it to keep their own history in step). */
  [[nodiscard]] std::uint64_t UndoGeneration() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_undoGeneration;
  }

  /** @brief Number of steps Undo() can undo. */
  [[nodiscard]] std::size_t UndoDepth() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_undo.size();
  }

  /** @brief Number of steps Redo() can redo. */
  [[nodiscard]] std::size_t RedoDepth() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_redo.size();
  }

  /** @brief Description of the step Undo() would undo (empty if none). */
  [[nodiscard]] std::string UndoLabel() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_undo.empty() ? std::string() : m_undo.back().label;
  }

  /** @brief Description of the step Redo() would redo (empty if none). */
  [[nodiscard]] std::string RedoLabel() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_redo.empty() ? std::string() : m_redo.back().label;
  }

  /** @brief Forgets the undo and redo history. */
  void ClearUndo() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_undo.clear();
    m_redo.clear();
    m_undoGroup.changes.clear();
    m_undoGroupBroken = m_undoGroupDepth > 0;
  }

  /** @brief Maximum number of undo steps kept (default 100; older steps are dropped). */
  void SetUndoLimit(std::size_t steps) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_undoLimit = std::max<std::size_t>(1, steps);
    while (m_undo.size() > m_undoLimit) m_undo.erase(m_undo.begin());
  }

  /** @brief Tells the backends that series were registered or changed. */
  void DataChanged() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_dataRevision = ++m_clock;
    changes::Result ignored;
    SyncAll({}, ignored);
  }

  /**
   * @brief Tells the session that entries were appended to a live series.
   *
   * Call after the provider's Size() has grown (from any thread). Bumps the
   * series revision, marks every layer showing the series (in any scene) as
   * changed, and syncs the backends, which redraw those layers only. The
   * global data revision is left alone: it means "the set of series changed"
   * (DataChanged) and makes backends redraw every layer. Entries already
   * converted stay cached.
   *
   * @param ref Series that grew. An empty source matches every source of
   *            that name.
   */
  void SeriesAppended(const data::SeriesRef &ref) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    const std::uint64_t tick = ++m_clock;
    m_seriesRevisions[{ref.name, ref.source}] = tick;
    for (const auto &[sceneName, scene] : m_options.scenes) {
      if (!scene)
        continue;
      for (const auto &[layer, series] : scene->layers.SeriesRefs()) {
        const bool match = series.name == ref.name &&
                           (ref.source.empty() || series.source.empty() ||
                            series.source == ref.source);
        if (match)
          m_revisions[sceneName].layer[layer] = tick;
      }
    }
    changes::Result ignored;
    SyncAll({}, ignored);
  }

  /**
   * @brief Revision of a series: the clock tick of its last SeriesAppended
   *        call, or 0 if it was never reported as grown.
   */
  std::uint64_t SeriesRevision(const data::SeriesRef &ref) const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_seriesRevisions.find({ref.name, ref.source});
    return it == m_seriesRevisions.end() ? 0 : it->second;
  }

  // ---- scenes and layout -------------------------------------------------

  /** @brief Names of all scenes, sorted. */
  std::vector<std::string> SceneNames() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
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
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_options.scenes.find(name);
    return it == m_options.scenes.end() ? nullptr : it->second;
  }

  /** @brief True if a scene of this name exists. */
  bool HasScene(const std::string &name) const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
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
    try {
      att.caps = b->GetCapabilities();
    } catch (...) {
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
    if (it == m_backends.end())
      return Fail(r, "unknown backend '" + id + "'");
    if (it->second.stopped)
      return Fail(r, "backend '" + id + "' has stopped.");
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
    CheckCapabilities(id, it->second, scene);
    SyncOne(id, it->second, {}, r);
    return r;
  }

  /** @brief Closes a scene in a backend. The scene and its state stay in the
   * session. */
  changes::Result Close(const std::string &id, const std::string &scene) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    changes::Result r;
    auto it = m_backends.find(id);
    if (it == m_backends.end())
      return Fail(r, "unknown backend '" + id + "'");
    if (it->second.stopped)
      return r;
    if (!it->second.open.erase(scene))
      return r;
    try {
      it->second.backend->CloseScene(scene);
    } catch (...) {
    }
    SyncOne(id, it->second, {}, r);
    return r;
  }

  /**
   * @brief Waits until backend @p id (or every backend, if empty) has finished
   *        the work it queued on its own thread. Call before reading outputs.
   */
  changes::Result Flush(const std::string &id = {}) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    changes::Result r;
    if (!id.empty() && !m_backends.count(id))
      return Fail(r, "unknown backend '" + id + "'");
    for (auto &[bid, att] : m_backends) {
      if ((!id.empty() && bid != id) || att.stopped)
        continue;
      try {
        att.backend->Flush();
      } catch (const std::exception &e) {
        Fail(r, "backend '" + bid + "' flush failed: " + e.what());
      }
    }
    return r;
  }

  /** @brief Draw counters of a backend, if it keeps them (tests). */
  std::optional<backend::DrawStats> BackendStats(const std::string &id) const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_backends.find(id);
    if (it == m_backends.end())
      return std::nullopt;
    return it->second.backend->Stats();
  }

  /** @brief Capabilities a backend declared at InitBackend, or nullopt. */
  std::optional<backend::Capabilities> BackendCapabilities(const std::string &id) const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_backends.find(id);
    if (it == m_backends.end())
      return std::nullopt;
    return it->second.caps;
  }

  /** @brief Ids of the attached backends, sorted. */
  std::vector<std::string> BackendIds() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<std::string> ids;
    for (const auto &[id, att] : m_backends)
      ids.push_back(id);
    return ids;
  }

  /** @brief Scenes open in a backend. */
  std::set<std::string> OpenScenes(const std::string &id) const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
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
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
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
    backend::Capabilities caps;        // queried once at InitBackend
    std::set<std::string> reported;    // "scene/layer" already warned about
  };

  // Bumps the camera revision of every scene sharing @p sceneName's camera and
  // syncs only backend @p id, as a non-origin sync, so it redraws the camera.
  void ResyncCamera(const std::string &id, const std::string &sceneName) {
    auto it = m_backends.find(id);
    const auto scene = FindScene(sceneName);
    if (it == m_backends.end() || !scene)
      return;
    const std::uint64_t tick = ++m_clock;
    for (const auto &[name, other] : m_options.scenes)
      if (other && (other == scene || (scene->camera && other->camera == scene->camera)))
        m_revisions[name].camera = tick;
    changes::Result ignored;
    SyncOne(id, it->second, {}, ignored);
  }

  // A scene was removed: close it wherever it is open and drop its revisions.
  void Forget(const std::string &sceneName) {
    for (auto &[id, att] : m_backends) {
      if (!att.open.erase(sceneName))
        continue;
      if (!att.stopped) {
        try {
          att.backend->CloseScene(sceneName);
        } catch (const std::exception &e) {
          Log(changes::Severity::Error,
              "backend '" + id + "' failed closing '" + sceneName + "': " + e.what());
        }
      }
      for (auto it = att.reported.begin(); it != att.reported.end();)
        it = it->rfind(sceneName + "/", 0) == 0 ? att.reported.erase(it) : std::next(it);
    }
    m_revisions.erase(sceneName);
  }

  // Warns (once per scene/layer) about layers whose element kind the backend
  // does not declare. Never fails: the backend simply does not draw them.
  void CheckCapabilities(const std::string &id, Attached &att,
                         const std::string &sceneName) {
    const auto scene = FindScene(sceneName);
    if (!scene)
      return;
    for (const auto &[layer, type] : scene->layers.Types()) {
      const std::string key = sceneName + "/" + layer;
      const auto kind = backend::KindOf(type);
      std::string problem;
      if (!kind)
        problem = "element type is not a built-in kind";
      else if (!att.caps.kinds.count(*kind))
        problem = std::string("element kind '") + backend::ToString(*kind) +
                  "' is not supported";
      if (problem.empty()) {
        att.reported.erase(key);
        continue;
      }
      if (!att.reported.insert(key).second)
        continue;
      Log(changes::Severity::Warning, id + ": scene '" + sceneName +
                                          "', layer '" + layer + "': " +
                                          problem + "; the layer is not drawn");
    }
  }

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
  std::map<std::pair<std::string, std::string>, std::uint64_t> m_seriesRevisions;
  std::uint64_t m_clock = 0;
  std::uint64_t m_layoutRevision = 0;
  std::uint64_t m_dataRevision = 0;

  std::map<std::string, Attached> m_backends;
  LogSink m_sink;
  PickHandler m_pickHandler;
  mutable std::recursive_mutex m_mutex;

  // Undo history (see Undo()).
  struct UndoStep {
    std::vector<std::shared_ptr<const changes::Change>> changes;  // inverses, in the order to apply
    std::string label;
  };
  enum class ReplayMode { None, Undo, Redo };
  std::vector<UndoStep> m_undo, m_redo;
  UndoStep m_undoGroup;
  int m_undoGroupDepth = 0;
  bool m_undoGroupUsed = false;
  bool m_undoGroupBroken = false;
  ReplayMode m_replay = ReplayMode::None;
  std::string m_replayLabel;
  std::size_t m_undoLimit = 100;
  std::uint64_t m_undoGeneration = 0;

  // A resolved batch's inverses: into the open group, or a step of its own.
  void RecordUndo(std::vector<std::shared_ptr<const changes::Change>> inverses, std::string label) {
    if (m_replay != ReplayMode::None) {  // undoing or redoing: the inverses form the opposite step
      auto &target = m_replay == ReplayMode::Undo ? m_redo : m_undo;
      target.push_back({std::move(inverses), m_replayLabel});
      return;
    }
    if (m_undoGroupDepth > 0) {
      if (m_undoGroupBroken) return;  // a change without inverse happened inside the group
      m_undoGroupUsed = true;
      // later batches are undone first
      inverses.insert(inverses.end(), m_undoGroup.changes.begin(), m_undoGroup.changes.end());
      m_undoGroup.changes = std::move(inverses);
      return;
    }
    PushUndo({std::move(inverses), std::move(label)}, true);
  }

  void PushUndo(UndoStep step, bool clearRedo) {
    m_undo.push_back(std::move(step));
    if (clearRedo) ++m_undoGeneration;
    if (clearRedo) m_redo.clear();
    while (m_undo.size() > m_undoLimit) m_undo.erase(m_undo.begin());
  }

  changes::Result DoReplay(ReplayMode what) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto &from = what == ReplayMode::Undo ? m_undo : m_redo;
    if (from.empty()) {
      changes::Result r;
      r.ok = false;
      r.messages.push_back(what == ReplayMode::Undo ? "nothing to undo" : "nothing to redo");
      return r;
    }
    UndoStep step = std::move(from.back());
    from.pop_back();
    std::vector<const changes::Change *> batch;
    for (const auto &c : step.changes) batch.push_back(c.get());
    m_replay = what;
    m_replayLabel = step.label;
    changes::Result r;
    try {
      r = Resolve(std::span<const changes::Change *const>(batch));
    } catch (...) {
      m_replay = ReplayMode::None;
      throw;
    }
    m_replay = ReplayMode::None;
    if (!r.ok) {  // the state no longer matches the history
      m_undo.clear();
      m_redo.clear();
    }
    return r;
  }
};

} // namespace fccvis::session
