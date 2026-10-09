#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Backend.h"
#include "BackendRegistry.h"

namespace fccvis::backend {

/**
 * @brief Pixel canvas backend (FV) for API testing and raw pixel drawing.
 */
class FVBackend final : public Backend {
 public:
  FVBackend() = default;
  ~FVBackend() override = default;

  [[nodiscard]] Capabilities GetCapabilities() const override;

  void Start(const std::string &id, Port &port, const Options &options) override;
  void Stop() override;

  bool OpenScene(const std::string &scene, const SessionView &view, std::string &error) override;
  void CloseScene(const std::string &scene) override;

  void Sync(const SyncRequest &request) override;

 private:
  // --- Canvas Primitives ---
  void ResizeCanvas(int width, int height);
  void ClearCanvas(std::uint32_t color = 0xFF000000); // ARGB black
  void SetPixel(int x, int y, std::uint32_t color);
  void DrawLine(int x0, int y0, int x1, int y1, std::uint32_t color);
  
  /// Export frame buffer to PPM format (P3 raw ASCII viewable by standard image tools)
  void ExportPPM(const std::string &filename) const;

  std::string m_id;
  Port *m_port = nullptr;

  // --- Frame Buffer State ---
  int m_width = 800;
  int m_height = 600;
  std::vector<std::uint32_t> m_pixels; // Flat array of ARGB pixels
  std::string m_outputPath = "fv_output.ppm";

  std::map<std::string, changes::SceneRevision> m_seenRevisions;
};

}  // namespace fccvis::backend
