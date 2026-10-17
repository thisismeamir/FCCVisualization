#pragma once
/**
 * @file Backend.h
 * @brief Contract between the session and a rendering target.
 *
 * Backends are thin bridges to existing libraries (ROOT/TEve, CED, Phoenix).
 * Only the session talks to a backend: it hands in read-only views and
 * receives changes and events through a Port, tagged with the backend id as
 * origin. A backend is scoped: it renders only the scenes opened in it, and
 * the layout is pruned to those scenes.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <typeindex>
#include <typeinfo>

#include "Changes.h"
#include "Elements.h"
#include "Data.h"
#include "Layout.h"
#include "Revision.h"
#include "Scene.h"

namespace fccvis::backend {

/** @brief Geometry kinds a backend may draw (defined in the data module). */
using data::ElementKind;
using data::KindOf;
using data::ToString;

/** @brief What a backend supports; also the source of its generated help text. */
struct Capabilities {
  std::string description;                 ///< One-line description of the target.
  std::set<ElementKind> kinds;             ///< Supported element kinds.
  std::set<std::string> styleProperties;   ///< Supported style properties ("opacity", "dash", ...).
  bool splits = true;                      ///< Supports split layouts.
  bool tabs = true;                        ///< Supports tabbed layouts.
  std::map<std::string, std::string> options;  ///< Accepted options: name -> description (help, completion).

  /** @brief Human-readable summary, generated from the fields above. */
  [[nodiscard]] std::string Help(const std::string& type) const {
    std::string s = type + " backend\n";
    if (!description.empty()) s += "  " + description + "\n";
    s += "  element kinds:";
    for (auto k : kinds) s += std::string(" ") + ToString(k);
    s += "\n  style properties:";
    for (const auto& p : styleProperties) s += " " + p;
    s += std::string("\n  layout: splits ") + (splits ? "yes" : "no") +
         ", tabs " + (tabs ? "yes" : "no") + "\n";
    if (!options.empty()) {
      s += "  options:\n";
      for (const auto& [name, what] : options) s += "    " + name + ": " + what + "\n";
    }
    return s;
  }
};

/**
 * @brief How often a backend rebuilt each part, for revision-diffing tests.
 *
 * Keys of the per-scene maps are scene names; layer keys are "scene/layer".
 */
struct DrawStats {
  std::uint64_t syncs = 0;                               ///< Sync calls that changed the target.
  std::uint64_t layouts = 0;                             ///< Times the layout was (re)applied.
  std::map<std::string, std::uint64_t> layers;           ///< "scene/layer" -> content rebuilds.
  std::map<std::string, std::uint64_t> cameras;          ///< scene -> camera applications.
  std::map<std::string, std::uint64_t> environments;     ///< scene -> environment applications.

  /** @brief Rebuilds of one layer (0 if never drawn). */
  [[nodiscard]] std::uint64_t Layer(const std::string& scene, const std::string& layer) const {
    auto it = layers.find(scene + "/" + layer);
    return it == layers.end() ? 0 : it->second;
  }
  /** @brief Camera applications of one scene (0 if never). */
  [[nodiscard]] std::uint64_t Camera(const std::string& scene) const {
    auto it = cameras.find(scene);
    return it == cameras.end() ? 0 : it->second;
  }
};

/** @brief Per-instance options passed at initBackend (window size, output path, ...). */
using Options = std::map<std::string, std::string>;

/** @brief A pick reported by a backend. */
struct PickEvent {
  std::string scene;       ///< Scene the pick happened in.
  std::string layer;       ///< Layer of the picked element.
  std::size_t entry = 0;   ///< Entry of the layer's series.
  std::size_t index = 0;   ///< Index of the element within the entry.
};

/** @brief Channel from a backend to the session. */
class Port {
 public:
  virtual ~Port() = default;

  /** @brief Submits a change on behalf of the backend (for example a manual camera move). */
  virtual changes::Result Submit(const changes::Change& change) = 0;

  /** @brief Logs a message through the session. */
  virtual void Log(changes::Severity severity, const std::string& message) = 0;

  /** @brief Reports a pick. */
  virtual void Picked(const PickEvent& event) = 0;

  /** @brief Reports that the user closed a scene's view in the backend. */
  virtual void SceneClosed(const std::string& scene) = 0;

  /** @brief Reports that the backend stopped by itself (for example its main window closed). */
  virtual void Stopped(const std::string& reason) = 0;
};

/** @brief Read-only access to the session state a backend needs. */
class SessionView {
 public:
  virtual ~SessionView() = default;
  /** @brief The session's series. */
  [[nodiscard]] virtual const data::SessionData& Data() const = 0;
  /** @brief A scene, or nullptr if it does not exist. */
  [[nodiscard]] virtual std::shared_ptr<const scene::Scene> FindScene(const std::string& name) const = 0;
  /** @brief Revision counters of a scene. */
  [[nodiscard]] virtual changes::SceneRevision Revision(const std::string& scene) const = 0;
  /** @brief Revision of the layout (and of the set of open scenes). */
  [[nodiscard]] virtual std::uint64_t LayoutRevision() const = 0;
  /** @brief Revision of the data (series registered or changed). */
  [[nodiscard]] virtual std::uint64_t DataRevision() const = 0;
};

/** @brief Everything a backend needs to bring itself up to date. */
struct SyncRequest {
  const SessionView& view;                                   ///< Current session state.
  const std::set<std::string>& open;                         ///< Scenes open in this backend.
  const std::optional<scene::layout::LayoutNode>& layout;    ///< Layout pruned to the open scenes.
  std::string origin;                                        ///< Backend that caused the change; empty if none.
};

/**
 * @brief A rendering target.
 *
 * Implementations register with the backend Registry. The session may call
 * from any thread and the calls must return quickly, so a backend whose
 * toolkit has its own thread (ROOT) marshals the work onto it.
 */
class Backend {
 public:
  virtual ~Backend() = default;

  /** @brief Supported features. */
  [[nodiscard]] virtual Capabilities GetCapabilities() const = 0;

  /** @brief Called once after creation. @p port stays valid until Stop. */
  virtual void Start(const std::string& id, Port& port, const Options& options) = 0;

  /** @brief Releases all resources. */
  virtual void Stop() = 0;

  /**
   * @brief Opens a scene (creates its view). Closing and reopening loses nothing,
   *        since view state lives in the scene.
   * @param scene Scene to open.
   * @param view  Session state, valid during the call only.
   * @param error Receives the reason on failure.
   * @return False if the scene cannot be opened, for example for an unsupported element kind.
   */
  virtual bool OpenScene(const std::string& scene, const SessionView& view, std::string& error) = 0;

  /** @brief Closes a scene's view. */
  virtual void CloseScene(const std::string& scene) = 0;

  /**
   * @brief Brings the backend up to date.
   *
   * Compare view revisions with the ones last drawn and update only what
   * differs. When @c request.origin is this backend, the change is already
   * on screen; record the new revisions without redrawing it.
   */
  virtual void Sync(const SyncRequest& request) = 0;

  /**
   * @brief Blocks until all work queued on the backend's own thread is done.
   *
   * Backends that render asynchronously (Sync returns after taking a snapshot)
   * override this; scripts and tests call it before reading outputs. The
   * default does nothing.
   */
  virtual void Flush() {}

  /**
   * @brief Draw counters for tests (contract: redraw only what changed).
   * @return Counters, or nullopt if the backend does not keep them.
   */
  [[nodiscard]] virtual std::optional<DrawStats> Stats() const { return std::nullopt; }
};

}  // namespace fccvis::backend
