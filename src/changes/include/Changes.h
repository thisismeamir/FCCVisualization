#pragma once
/**
 * @file Change.h
 * @brief Base of the change catalogue and the result of resolving changes.
 *
 * A change is a small struct that knows its name, arguments, footprint and how
 * to apply itself. Changes are executed only through Session::Resolve, which
 * adds logging, revisions and backend notification. Each change validates
 * before it mutates, so it is all-or-nothing.
 */

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include "Data.h"
#include "FootPrint.h"
#include "Layout.h"
#include "Scene.h"

namespace fccvis::changes {

/** @brief Log severity. */
enum class Severity { Info, Warning, Error };

/** @brief Thrown by a change that cannot be applied. */
class ChangeError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

/** @brief Registry of scenes, as held by the session. */
using SceneMap = std::unordered_map<std::string, std::shared_ptr<scene::Scene>>;

/** @brief What a change may modify. A view onto the session's state. */
struct Workspace {
  data::SessionData& data;                             ///< Series store.
  SceneMap& scenes;                                    ///< Scene registry.
  std::optional<scene::layout::LayoutNode>& layout;    ///< Session layout.
};

/** @brief Outcome of resolving one change or a batch. */
struct Result {
  bool ok = true;                       ///< False if a change failed.
  std::size_t applied = 0;              ///< Number of changes applied.
  std::vector<Footprint> touched;       ///< Footprints of the applied changes.
  std::vector<std::string> messages;    ///< Errors and warnings.

  /** @brief True if @ref ok. */
  explicit operator bool() const noexcept { return ok; }
};

/** @brief Base of every change. */
class Change {
 public:
  virtual ~Change() = default;

  /** @brief Short stable name, for example "AddLayer". */
  [[nodiscard]] virtual std::string Name() const = 0;

  /** @brief Name with arguments, for logs and scripts. */
  [[nodiscard]] virtual std::string Describe() const = 0;

  /** @brief Targeted scene; empty for session-wide changes. */
  [[nodiscard]] virtual std::string SceneName() const { return {}; }

  /** @brief What the change touches once applied. */
  [[nodiscard]] virtual Footprint Touches() const = 0;

  /**
   * @brief Applies the change.
   * @throws ChangeError if it cannot be applied; nothing is modified then.
   */
  virtual void Apply(Workspace& ws) const = 0;
};

}  // namespace fccvis::changes
