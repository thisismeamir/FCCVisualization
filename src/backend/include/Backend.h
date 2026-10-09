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
 */

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>

#include "Changes.h"
#include "Data.h"
#include "Layout.h"
#include "Revision.h"
#include "Scene.h"

namespace fccvis::backend {

/** @brief Geometry kinds a backend may draw. */
enum class ElementKind { Point, Line, Surface };

/** @brief Name of an element kind. */
[[nodiscard]] inline const char* ToString(ElementKind k) {
  switch (k) {
    case ElementKind::Point: return "point";
    case ElementKind::Line: return "line";
    case ElementKind::Surface: return "surface";
  }
  return "?";
}

/** @brief What a backend supports; also the source of its generated help text. */
struct Capabilities {
  std::string description;                 ///< One-line description of the target.
  std::set<ElementKind> kinds;             ///< Supported element kinds.
  std::set<std::string> styleProperties;   ///< Supported style properties ("opacity", "dash", ...).
  bool splits = true;                      ///< Supports split layouts.
  bool tabs = true;                        ///< Supports tabbed layouts.

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
    return s;
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
  [[nodiscard]] virtual const data::SessionData& Data() const = 0;
  [[nodiscard]] virtual std::shared_ptr<const scene::Scene> FindScene(const std::string& name) const = 0;
  [[nodiscard]] virtual changes::SceneRevision Revision(const std::string& scene) const = 0;
  [[nodiscard]] virtual std::uint64_t LayoutRevision() const = 0;
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
};

}  // namespace fccvis::backend
