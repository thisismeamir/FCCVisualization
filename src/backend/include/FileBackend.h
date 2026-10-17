/**
 * @file FileBackend.h
 * @brief Base class of backends that write files (raster PNG, SVG, histograms).
 *
 * Implements the Backend contract once: option parsing (`width`, `height`,
 * `output_dir`, `tolerance`, `async`), revision diffing with RevisionTracker,
 * retained per-layer drawings (draw::RetainedScenes), DrawStats, and
 * rendering on an Executor thread with coalescing (a burst of syncs renders
 * the last state once). A derived class declares its capabilities and writes
 * its files in Render(); it may read extra options in Configure().
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include "Backend.h"
#include "Drawing.h"
#include "Executor.h"
#include "RevisionTracker.h"

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>

namespace fccvis::backend {

/** @brief Backend that renders open scenes to files after each changing sync. */
class FileBackend : public Backend {
 public:
  /** @brief Everything a render needs; a snapshot, independent of the session. */
  struct Job {
    std::map<std::string, draw::SceneDrawing> scenes;     ///< Drawings of the open scenes.
    std::optional<scene::layout::LayoutNode> layout;      ///< Pruned layout, if any.
    std::set<std::string> open;                           ///< Open scenes.
    std::uint64_t generation = 0;                         ///< Increases with every changing sync.
    std::shared_ptr<const void> extra;                    ///< What Collect() returned for this sync.
  };

  ~FileBackend() override;

  void Start(const std::string& id, Port& port, const Options& options) override;
  void Stop() override;
  bool OpenScene(const std::string& scene, const SessionView& view, std::string& error) override;
  void CloseScene(const std::string& scene) override;
  void Sync(const SyncRequest& request) override;
  void Flush() override;
  [[nodiscard]] std::optional<DrawStats> Stats() const override;

  /** @brief Number of renders completed (tests). */
  [[nodiscard]] std::uint64_t Renders() const;
  /** @brief `<output_dir>/<scene><extension>` (scene name sanitized for file systems). */
  [[nodiscard]] std::string ScenePath(const std::string& scene, const std::string& extension) const;
  /** @brief `<output_dir>/<name>`. */
  [[nodiscard]] std::string OutputPath(const std::string& name) const;

 protected:
  /** @brief Short name used in log messages ("raster", "svg"). */
  [[nodiscard]] virtual std::string Kind() const = 0;
  /** @brief Reads backend-specific options; throw std::invalid_argument for bad values. */
  virtual void Configure(const Options& options) { (void)options; }
  /** @brief Element kinds turned into drawings (default: the capabilities' kinds). */
  [[nodiscard]] virtual std::set<ElementKind> DrawnKinds() const { return GetCapabilities().kinds; }
  /**
   * @brief Extracts extra data from the session during Sync (session thread),
   *        for backends that need more than drawings; stored in Job::extra.
   *        Called only when the plan is not empty or a scene was closed.
   */
  virtual std::shared_ptr<const void> Collect(const SyncPlan& plan, const SyncRequest& request) {
    (void)plan;
    (void)request;
    return nullptr;
  }
  /** @brief Writes the files of one job (render thread). */
  virtual void Render(const Job& job) = 0;
  /** @brief Logs through the port (safe from the render thread). */
  void Log(changes::Severity s, const std::string& m) const;

  /** @brief The port given at Start (nullptr after Stop). Use from the session's or the render thread only. */
  [[nodiscard]] Port* GetPort() const { return m_port; }
  /** @brief Instance id given at Start. */
  [[nodiscard]] const std::string& Id() const { return m_id; }

  /** @brief Integer option in 1..16384, or @p fallback when absent. */
  [[nodiscard]] static int IntOption(const Options& o, const char* key, int fallback);

  int m_width = 800;           ///< Picture width (pixels).
  int m_height = 600;          ///< Picture height (pixels).
  std::string m_outputDir = "."; ///< Output directory.
  double m_tolerance = 0.02;   ///< Surface tessellation tolerance.
  bool m_async = true;         ///< Render on the executor thread.

 private:
  std::string m_id;
  Port* m_port = nullptr;
  RevisionTracker m_tracker;
  std::unique_ptr<draw::RetainedScenes> m_retained;
  mutable std::mutex m_mutex;  // guards m_stats, m_pending, m_rendered, m_renders, m_port use
  DrawStats m_stats;
  std::shared_ptr<const Job> m_pending;
  std::uint64_t m_generation = 0;
  std::uint64_t m_rendered = 0;
  std::uint64_t m_renders = 0;
  bool m_closed = false;  // a scene was closed: the next sync renders even if nothing else changed
  std::unique_ptr<Executor> m_executor;
};

}  // namespace fccvis::backend
