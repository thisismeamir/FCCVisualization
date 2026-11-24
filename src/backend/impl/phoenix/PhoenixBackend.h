/**
 * @file PhoenixBackend.h
 * @brief B5: Phoenix web event display: event-data JSON, OBJ geometry, optional HTTP host.
 *
 * Registered as "phoenix". After each changing sync it writes (Session::Flush waits):
 * - `<output_dir>/events.json`: Phoenix event-data JSON with one event per open
 *   scene (key = scene name). Point layers become `Hits` collections (objects
 *   of type `Point` with `pos` [x, y, z] and `color`; Phoenix applies the first
 *   hit's colour to a collection, so a layer whose points have several colours
 *   is split into one collection per colour); line layers become `Tracks`
 *   (`pos` = list of [x, y, z], `color`, `linewidth`). Metadata attributes
 *   are added to each object as extra keys (Phoenix shows them and uses
 *   `phi`, `eta`, `energy`, `pT`, ... for cuts). `event number` is the entry
 *   of the scene's first layer.
 * - `<output_dir>/<scene>.obj` + `.mtl` for surface layers (Wavefront OBJ in
 *   millimetres, one object per layer and colour), to load as geometry.
 * With option `port` (0 = off, default) an HTTP server serves these files
 * with CORS headers, so Phoenix can load `http://<host>:<port>/events.json`
 * (URL options `?file=<url>&type=json`) and reload it when it changes.
 *
 * Format verified against the Phoenix sources (guides/developers/
 * event_data_format.md and loaders/objects/phoenix-objects.ts at commit
 * c770587, 2026-10-09). Not tested against a running Phoenix instance.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include "BackendRegistry.h"
#include "FileBackend.h"
#include "HttpServer.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace fccvis::backend {

/** @brief Phoenix backend (B5). */
class PhoenixBackend final : public FileBackend {
 public:
  ~PhoenixBackend() override;
  [[nodiscard]] Capabilities GetCapabilities() const override;
  void Stop() override;

  /** @brief Metadata of the selected elements of one layer, by element index. */
  using LayerMeta = std::map<std::size_t, data::Metadata>;
  /** @brief scene -> layer -> metadata, plus each scene's event number. */
  struct Extracted {
    std::map<std::string, std::map<std::string, LayerMeta>> meta;  ///< Metadata by scene and layer.
    std::map<std::string, std::size_t> eventNumber;                 ///< Entry of each scene's first layer.
  };

  /** @brief Phoenix event-data JSON for the given drawings (also used by tests). */
  [[nodiscard]] static std::string EventsJson(const std::map<std::string, draw::SceneDrawing>& scenes,
                                              const std::set<std::string>& open, const Extracted& extracted);

 protected:
  [[nodiscard]] std::string Kind() const override { return "phoenix"; }
  void Configure(const Options& options) override;
  std::shared_ptr<const void> Collect(const SyncPlan& plan, const SyncRequest& request) override;
  void Render(const Job& job) override;

 private:
  int m_port = 0;
  std::string m_bind = "127.0.0.1";
  std::unique_ptr<phoenix::HttpServer> m_server;
};

}  // namespace fccvis::backend
