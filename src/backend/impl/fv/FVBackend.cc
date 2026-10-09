#include "FVBackend.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>

namespace fccvis::backend {

Capabilities FVBackend::GetCapabilities() const {
  Capabilities c;
  c.description = "FV Pixel Canvas Backend for raw graphics rendering.";
  c.kinds = {ElementKind::Point, ElementKind::Line, ElementKind::Surface};
  c.styleProperties = {"color", "opacity", "visible", "size", "width"};
  c.splits = false;
  c.tabs = false;
  return c;
}

void FVBackend::Start(const std::string &id, Port &port, const Options &options) {
  m_id = id;
  m_port = &port;

  // Extract custom canvas options if supplied
  if (auto it = options.find("width"); it != options.end()) {
    m_width = std::stoi(it->second);
  }
  if (auto it = options.find("height"); it != options.end()) {
    m_height = std::stoi(it->second);
  }
  if (auto it = options.find("output"); it != options.end()) {
    m_outputPath = it->second;
  }

  ResizeCanvas(m_width, m_height);
  m_port->Log(changes::Severity::Info, "FVBackend initialized canvas (" +
                                           std::to_string(m_width) + "x" +
                                           std::to_string(m_height) + ")");
}

void FVBackend::Stop() {
  if (m_port) {
    m_port->Log(changes::Severity::Info, "FVBackend stopped.");
  }
  m_port = nullptr;
}

bool FVBackend::OpenScene(const std::string &scene, const SessionView &view, std::string &error) {
  if (!view.FindScene(scene)) {
    error = "Scene '" + scene + "' does not exist.";
    return false;
  }
  return true;
}

void FVBackend::CloseScene(const std::string &scene) {
  m_seenRevisions.erase(scene);
}

// --- Canvas Drawing Implementation ---

void FVBackend::ResizeCanvas(int width, int height) {
  m_width = width;
  m_height = height;
  m_pixels.assign(m_width * m_height, 0xFF000000); // Default black canvas
}

void FVBackend::ClearCanvas(std::uint32_t color) {
  std::fill(m_pixels.begin(), m_pixels.end(), color);
}

void FVBackend::SetPixel(int x, int y, std::uint32_t color) {
  if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
    m_pixels[y * m_width + x] = color;
  }
}

void FVBackend::DrawLine(int x0, int y0, int x1, int y1, std::uint32_t color) {
  int dx = std::abs(x1 - x0);
  int sx = x0 < x1 ? 1 : -1;
  int dy = -std::abs(y1 - y0);
  int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;

  while (true) {
    SetPixel(x0, y0, color);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void FVBackend::ExportPPM(const std::string &filename) const {
  std::ofstream out(filename);
  if (!out.is_open()) return;

  out << "P3\n" << m_width << " " << m_height << "\n255\n";
  for (int y = 0; y < m_height; ++y) {
    for (int x = 0; x < m_width; ++x) {
      std::uint32_t pixel = m_pixels[y * m_width + x];
      int r = (pixel >> 16) & 0xFF;
      int g = (pixel >> 8) & 0xFF;
      int b = pixel & 0xFF;
      out << r << " " << g << " " << b << " ";
    }
    out << "\n";
  }
}

// --- Sync & Scene Drawing ---

void FVBackend::Sync(const SyncRequest &request) {
  if (request.origin == m_id) {
    for (const auto &scene : request.open) {
      m_seenRevisions[scene] = request.view.Revision(scene);
    }
    return;
  }

  ClearCanvas(0xFF1E1E1E); // Clear to dark gray background

  for (const auto &sceneName : request.open) {
    const auto currentRev = request.view.Revision(sceneName);

    // Draw raw canvas content / test shapes
    DrawLine(0, 0, m_width - 1, m_height - 1, 0x00FF00FF);     // Magenta diagonal
    DrawLine(0, m_height - 1, m_width - 1, 0, 0x0000FFFF);     // Yellow diagonal

    // Fetch scene data via request.view.Data() to iterate and render points/lines...

    m_seenRevisions[sceneName] = currentRev;
  }

  // Dump frame buffer image file upon sync
  ExportPPM(m_outputPath);
}

static Registrar g_fvBackendRegistrar("fv", [] {
  return std::make_unique<FVBackend>();
});

}  // namespace fccvis::backend
