/**
 * @file RootBackend.h
 * @brief B1: ROOT classic TEve viewer (decision in docs/design/root-experiments.md).
 *
 * Registered as "root". Every instance shows its open scenes in its own tab
 * of the one Eve browser window (process-wide runtime, one ROOT thread):
 * - each scene is a TEveScene; each layer an element list holding TEvePointSet
 *   groups (one per resolved point style), TEveLine per polyline and
 *   TEveTriangleSet per surface, built from the layer drawings (draw::BuildLayer)
 *   and rebuilt only when the layer's revision moved;
 * - the pruned layout becomes TEveWindowPack (splits; weights are not
 *   honoured, Eve packs share space equally) and TEveWindowTab (tabs) with
 *   one TEveViewer per pane, titled by the layout label;
 * - the camera direction selects an orthographic view when it is along an
 *   axis, a perspective view otherwise; magnification zooms.
 *
 * Events back to the session (through the Port, from a separate thread so
 * the session never waits on ROOT): picks (`Port::Picked`, point index from
 * Eve's secondary selection), camera rotations in a viewer
 * (`ChangeCameraDirection` with this backend as origin, rejected for Fixed
 * cameras), and closing the Eve window (`Port::Stopped`).
 *
 * Options: `picture_dir` writes `<dir>/<scene>.png` after every update
 * (`TGLViewer::SavePictureUsingFBO`, `width` x `height`, default 800x600).
 * Needs an X display (`DISPLAY`); Start fails with a clear message otherwise.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include "BackendRegistry.h"
#include "Executor.h"
#include "FileBackend.h"

#include <cstddef>
#include <map>
#include <memory>
#include <string>

namespace fccvis::backend {

/** @brief Classic TEve backend (B1). */
class RootBackend final : public FileBackend {
 public:
  RootBackend();
  ~RootBackend() override;
  [[nodiscard]] Capabilities GetCapabilities() const override;
  void Stop() override;
  void Flush() override;

  struct State;  ///< ROOT-side objects, touched only on the ROOT thread.
  /** @brief Entry drawn per scene and layer (for pick events). */
  using Entries = std::map<std::string, std::map<std::string, std::size_t>>;

 protected:
  [[nodiscard]] std::string Kind() const override { return "root"; }
  void Configure(const Options& options) override;
  [[nodiscard]] std::shared_ptr<const void> Collect(const SyncPlan& plan, const SyncRequest& request) override;
  void Render(const Job& job) override;

 private:
  std::shared_ptr<State> m_state;
  std::unique_ptr<Executor> m_events;  // delivers events to the Port off the ROOT thread
  std::string m_pictureDir;
};

}  // namespace fccvis::backend
