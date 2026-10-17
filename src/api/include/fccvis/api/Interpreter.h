/**
 * @file Interpreter.h
 * @brief Command interpreter behind the shell, scripts and configuration files.
 *
 * One command line is one command: a query, or one batch of changes resolved
 * through Session::Resolve (no other mutation path). The interpreter is UI
 * free: it returns Replies (output lines and errors) and keeps the journal of
 * state-changing commands that `save` writes. The `fccvis-shell` program adds
 * line editing, completion display and asynchronous event printing on top.
 *
 * Commands (see `help` for the generated reference):
 * `load`, `series`, `describe`, `scene`, `layer`, `where`, `style`, `env`,
 * `camera`, `layout`, `entry`, `backend`, `open`, `close`, `log`, `save`,
 * `source`, `echo`, `undo`, `redo`, `help`, `quit`. Lines starting with `#`
 * are comments.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include "Adapter.h"
#include "fccvis/api/Expression.h"
#include "Session.h"

#include <cstddef>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace fccvis::api {

/** @brief Result of one command (or of a script). */
struct Reply {
  bool ok = true;                   ///< False if the command failed.
  std::vector<std::string> lines;   ///< Output.
  std::vector<std::string> errors;  ///< Error messages (with position markers for parse errors).
  bool quit = false;                ///< `quit` was requested.

  /** @brief Output and errors as text (errors prefixed with "error: "). */
  [[nodiscard]] std::string Text() const;
};

/** @brief A source opened with `load` (kept so that `save` can reopen it). */
struct LoadRecord {
  std::string adapter;        ///< Adapter name.
  std::string path;           ///< Path as given.
  adapters::Options options;  ///< Options as given.
};

/** @brief Kind of an argument, for completion and help. */
enum class ArgKind {
  Keyword,        ///< One of a fixed word list.
  Name,           ///< A new name (no completion).
  Scene,          ///< Existing scene.
  LayerPath,      ///< scene/layer.
  Series,         ///< Registered series name.
  Adapter,        ///< Adapter name.
  AdapterOption,  ///< key=value option of the adapter named earlier on the line.
  File,           ///< File path.
  BackendType,    ///< Registered backend type.
  BackendId,      ///< Attached backend id.
  BackendOption,  ///< key=value option of the backend type named earlier on the line.
  Filter,         ///< Filter expression (rest of the line).
  Style,          ///< Style assignments for the layer named earlier (rest of the line).
  Environment,    ///< Environment assignments (rest of the line).
  Layout,         ///< Layout expression (rest of the line).
  Number,         ///< A number.
  Topic,          ///< Help topic.
  Text            ///< Free text (rest of the line).
};

/** @brief One form of a command: words and argument kinds, for help and completion. */
struct CommandForm {
  std::vector<std::string> words;     ///< Fixed leading words, for example {"scene", "new"}.
  std::vector<ArgKind> args;          ///< Arguments after the words.
  std::vector<std::string> keywords;  ///< Allowed words for ArgKind::Keyword arguments (all of them).
  std::string usage;                  ///< Usage line.
  std::string summary;                ///< One-line description.
};

/** @brief Command interpreter on a session. */
class Interpreter {
 public:
  /** @brief Attaches to @p session and installs a log sink (see SetLogSink). */
  explicit Interpreter(session::Session& session);
  ~Interpreter();
  Interpreter(const Interpreter&) = delete;
  Interpreter& operator=(const Interpreter&) = delete;

  /** @brief Executes one command line. */
  Reply Execute(std::string_view line);

  /**
   * @brief Executes a script: one command per line; a trailing `\` continues
   *        a line. Stops at the first failing command when @p stopOnError.
   *        Errors are prefixed with the line number.
   */
  Reply Run(std::string_view script, bool stopOnError = true);

  /**
   * @brief Receives session log messages (from any thread) at or above
   *        @p minimum severity. Every message is also kept for `log`.
   */
  void SetLogSink(std::function<void(changes::Severity, const std::string&)> sink,
                  changes::Severity minimum = changes::Severity::Warning);

  /**
   * @brief Directory relative input names (load, source) resolve against;
   *        `source` sets it to the sourced file's directory while it runs.
   *        Outputs (save, backend output_dir) are relative to the working directory.
   */
  void SetBaseDirectory(std::string directory) { m_base = std::move(directory); }

  /** @brief Whether `source` stops at the first failing command (default true). */
  void SetStopOnError(bool stop) { m_stopOnError = stop; }

  /** @brief Completion candidates for the last word of @p line (sorted, unique). */
  [[nodiscard]] std::vector<std::string> Complete(std::string_view line) const;

  /** @brief Help text: overview, a command, `adapter <name>`, `backend <type>`, `filters`, `styles`, `layouts`. */
  [[nodiscard]] std::string Help(const std::string& topic = {}) const;

  /**
   * @brief Reference documentation in Markdown, generated from the command
   *        table and the adapter and backend registries (`fccvis-shell
   *        --reference`; the Sphinx reference pages are built from it).
   * @param section "commands", "adapters", "backends" or empty for all.
   */
  [[nodiscard]] static std::string Reference(const std::string& section = {});

  /** @brief Command forms (help and completion data). */
  [[nodiscard]] static const std::vector<CommandForm>& Forms();

  /** @brief Sources opened with `load`, in order. */
  [[nodiscard]] const std::vector<LoadRecord>& Loads() const { return m_loads; }
  /** @brief Successful state-changing commands, in canonical form, in order. */
  [[nodiscard]] const std::vector<std::string>& Journal() const { return m_journal; }
  /** @brief Commands that rebuild the current state: the journal without camera placement, then every scene's current camera. */
  [[nodiscard]] std::vector<std::string> StateCommands() const;

  /** @brief The session. */
  [[nodiscard]] session::Session& GetSession() { return m_session; }

 private:
  struct Line;
  struct Shared;
  Reply Dispatch(Line& line);
  std::string Resolve(const std::string& path) const;

  session::Session& m_session;
  std::shared_ptr<Shared> m_shared;  // log ring and sink, shared with the session's sink
  std::string m_base;
  std::vector<LoadRecord> m_loads;
  std::vector<std::string> m_journal;
  std::map<std::string, std::string> m_filters;               // scene/layer -> expression
  std::map<std::string, std::vector<std::string>> m_styles;   // scene/layer -> rule texts
  int m_sourceDepth = 0;
  bool m_stopOnError = true;
  struct Snapshot {  // interpreter state before an undo step
    std::vector<std::string> journal;
    std::map<std::string, std::string> filters;
    std::map<std::string, std::vector<std::string>> styles;
  };
  std::vector<Snapshot> m_past, m_future;
};

}  // namespace fccvis::api
