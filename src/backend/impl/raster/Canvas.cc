// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
#include "Canvas.h"

#include <algorithm>
#include <cmath>
#include <type_traits>
#include <variant>
#include <utility>
#include <vector>

namespace fccvis::backend::raster {

Canvas::Canvas(int w, int h, style::Color background) {
  m_img.width = std::max(1, w);
  m_img.height = std::max(1, h);
  m_img.rgba.resize(static_cast<std::size_t>(m_img.width) * m_img.height * 4);
  Clip({0, 0, static_cast<double>(m_img.width), static_cast<double>(m_img.height)});
  FillRect({0, 0, static_cast<double>(m_img.width), static_cast<double>(m_img.height)}, background);
}

void Canvas::Clip(const draw::Rect& r) {
  m_cx0 = std::clamp(static_cast<int>(std::floor(r.x)), 0, m_img.width);
  m_cy0 = std::clamp(static_cast<int>(std::floor(r.y)), 0, m_img.height);
  m_cx1 = std::clamp(static_cast<int>(std::floor(r.x + r.w)), 0, m_img.width);
  m_cy1 = std::clamp(static_cast<int>(std::floor(r.y + r.h)), 0, m_img.height);
}

void Canvas::Blend(int x, int y, style::Color c, float alpha) {
  if (x < m_cx0 || y < m_cy0 || x >= m_cx1 || y >= m_cy1 || alpha <= 0) return;
  auto* p = m_img.rgba.data() + (static_cast<std::size_t>(y) * m_img.width + x) * 4;
  const float a = std::min(alpha, 1.f);
  p[0] = static_cast<std::uint8_t>(std::lround(c.r * a + p[0] * (1 - a)));
  p[1] = static_cast<std::uint8_t>(std::lround(c.g * a + p[1] * (1 - a)));
  p[2] = static_cast<std::uint8_t>(std::lround(c.b * a + p[2] * (1 - a)));
  p[3] = 255;
}

void Canvas::FillRect(const draw::Rect& r, style::Color c, float alpha) {
  const int x0 = std::max(m_cx0, static_cast<int>(std::floor(r.x)));
  const int y0 = std::max(m_cy0, static_cast<int>(std::floor(r.y)));
  const int x1 = std::min(m_cx1, static_cast<int>(std::floor(r.x + r.w)));
  const int y1 = std::min(m_cy1, static_cast<int>(std::floor(r.y + r.h)));
  for (int y = y0; y < y1; ++y)
    for (int x = x0; x < x1; ++x) Blend(x, y, c, alpha * (c.a / 255.f));
}

void Canvas::Triangle(double x0, double y0, double x1, double y1, double x2, double y2, style::Color c,
                      float alpha) {
  const double area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
  if (area == 0) return;
  const int minx = std::max(m_cx0, static_cast<int>(std::floor(std::min({x0, x1, x2}))));
  const int maxx = std::min(m_cx1 - 1, static_cast<int>(std::ceil(std::max({x0, x1, x2}))));
  const int miny = std::max(m_cy0, static_cast<int>(std::floor(std::min({y0, y1, y2}))));
  const int maxy = std::min(m_cy1 - 1, static_cast<int>(std::ceil(std::max({y0, y1, y2}))));
  const double s = area > 0 ? 1.0 : -1.0;
  for (int y = miny; y <= maxy; ++y)
    for (int x = minx; x <= maxx; ++x) {
      const double px = x + 0.5, py = y + 0.5;
      const double w0 = ((x1 - px) * (y2 - py) - (x2 - px) * (y1 - py)) * s;
      const double w1 = ((x2 - px) * (y0 - py) - (x0 - px) * (y2 - py)) * s;
      const double w2 = ((x0 - px) * (y1 - py) - (x1 - px) * (y0 - py)) * s;
      if (w0 >= 0 && w1 >= 0 && w2 >= 0) Blend(x, y, c, alpha);
    }
}

void Canvas::Marker(style::MarkerShape shape, double x, double y, double size, style::Color c, float alpha) {
  const double r = std::max(size, 1.0) / 2;
  const int x0 = static_cast<int>(std::floor(x - r)), x1 = static_cast<int>(std::ceil(x + r));
  const int y0 = static_cast<int>(std::floor(y - r)), y1 = static_cast<int>(std::ceil(y + r));
  if (r <= 0.75) {  // tiny: one pixel
    Blend(static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)), c, alpha);
    return;
  }
  switch (shape) {
    case style::MarkerShape::Triangle:
      Triangle(x, y - r, x - r, y + r, x + r, y + r, c, alpha);
      return;
    case style::MarkerShape::Cross: {
      const double w = std::max(1.0, r / 2.5);
      Solid(x - r, y - r, x + r, y + r, w, c, alpha);
      Solid(x - r, y + r, x + r, y - r, w, c, alpha);
      return;
    }
    default:
      break;
  }
  for (int py = y0; py <= y1; ++py)
    for (int px = x0; px <= x1; ++px) {
      const double dx = px + 0.5 - x, dy = py + 0.5 - y;
      bool in = false;
      switch (shape) {
        case style::MarkerShape::Square: in = std::abs(dx) <= r && std::abs(dy) <= r; break;
        case style::MarkerShape::Diamond: in = std::abs(dx) + std::abs(dy) <= r; break;
        default: in = dx * dx + dy * dy <= r * r; break;
      }
      if (in) Blend(px, py, c, alpha);
    }
}

void Canvas::Solid(double x0, double y0, double x1, double y1, double width, style::Color c, float alpha) {
  const double dx = x1 - x0, dy = y1 - y0;
  const double len = std::hypot(dx, dy);
  if (width <= 1.5) {  // thin: DDA, one pixel per step
    const int steps = std::max(1, static_cast<int>(std::ceil(std::max(std::abs(dx), std::abs(dy)))));
    int lastX = -1, lastY = -1;
    for (int i = 0; i <= steps; ++i) {
      const int px = static_cast<int>(std::floor(x0 + dx * i / steps));
      const int py = static_cast<int>(std::floor(y0 + dy * i / steps));
      if (px == lastX && py == lastY) continue;
      Blend(px, py, c, alpha);
      lastX = px;
      lastY = py;
    }
    return;
  }
  if (len == 0) {
    Marker(style::MarkerShape::Circle, x0, y0, width, c, alpha);
    return;
  }
  const double nx = -dy / len * width / 2, ny = dx / len * width / 2;
  Triangle(x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, c, alpha);
  Triangle(x0 + nx, y0 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny, c, alpha);
}

void Canvas::Line(double x0, double y0, double x1, double y1, double width, style::Color c, float alpha,
                  style::DashPattern dash, double& phase) {
  if (dash == style::DashPattern::Solid) {
    Solid(x0, y0, x1, y1, width, c, alpha);
    return;
  }
  std::vector<std::pair<double, bool>> pattern;  // length, drawn
  switch (dash) {
    case style::DashPattern::Dashed: pattern = {{6, true}, {4, false}}; break;
    case style::DashPattern::Dotted: pattern = {{1.5, true}, {3, false}}; break;
    default: pattern = {{6, true}, {3, false}, {1.5, true}, {3, false}}; break;
  }
  double period = 0;
  for (const auto& [l, on] : pattern) period += l * std::max(1.0, width);
  const double len = std::hypot(x1 - x0, y1 - y0);
  double t = 0;
  while (t < len) {
    double local = std::fmod(phase + t, period), acc = 0;
    double segEnd = len;
    bool on = true;
    for (const auto& [l, d] : pattern) {
      const double sl = l * std::max(1.0, width);
      if (local < acc + sl) {
        on = d;
        segEnd = std::min(len, t + (acc + sl - local));
        break;
      }
      acc += sl;
    }
    if (on) {
      const double a = t / len, b = segEnd / len;
      Solid(x0 + (x1 - x0) * a, y0 + (y1 - y0) * a, x0 + (x1 - x0) * b, y0 + (y1 - y0) * b, width, c, alpha);
    }
    t = segEnd + 1e-9;
  }
  phase = std::fmod(phase + len, period);
}

void RenderScene(Canvas& canvas, const draw::SceneDrawing& scene, const draw::Rect& rect) {
  canvas.Clip(rect);
  for (const auto& op : draw::Paint(scene, rect)) {
    std::visit(
        [&](const auto& sh) {
          using T = std::decay_t<decltype(sh)>;
          if constexpr (std::is_same_v<T, draw::PaintFill>) {
            canvas.FillRect(sh.rect, sh.color);
          } else if constexpr (std::is_same_v<T, draw::PaintTriangle>) {
            canvas.Triangle(sh.p[0].x, sh.p[0].y, sh.p[1].x, sh.p[1].y, sh.p[2].x, sh.p[2].y, sh.color, sh.alpha);
          } else if constexpr (std::is_same_v<T, draw::PaintPolyline>) {
            double phase = 0;
            for (std::size_t i = 0; i + 1 < sh.p.size(); ++i)
              canvas.Line(sh.p[i].x, sh.p[i].y, sh.p[i + 1].x, sh.p[i + 1].y, sh.width, sh.color, sh.alpha, sh.dash,
                          phase);
          } else {
            canvas.Marker(sh.shape, sh.p.x, sh.p.y, sh.size, sh.color, sh.alpha);
          }
        },
        op.shape);
  }
  canvas.Clip({0, 0, static_cast<double>(canvas.Pixels().width), static_cast<double>(canvas.Pixels().height)});
}

}  // namespace fccvis::backend::raster
