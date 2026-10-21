// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
#include "Svg.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <type_traits>
#include <variant>

namespace fccvis::backend::svg {

namespace {

std::string Hex(style::Color c) {
  char buf[8];
  std::snprintf(buf, sizeof buf, "#%02x%02x%02x", c.r, c.g, c.b);
  return buf;
}

std::string Points(const std::vector<draw::Projected>& p, int prec) {
  std::string s;
  for (std::size_t i = 0; i < p.size(); ++i) s += (i ? " " : "") + Num(p[i].x, prec) + "," + Num(p[i].y, prec);
  return s;
}

std::string Opacity(const char* attr, float a) {
  return a >= 0.9995f ? std::string() : std::string(" ") + attr + "=\"" + Num(a, 3) + "\"";
}

std::string Dash(style::DashPattern d, double w) {
  const double u = std::max(1.0, w);
  switch (d) {
    case style::DashPattern::Solid: return {};
    case style::DashPattern::Dashed: return " stroke-dasharray=\"" + Num(6 * u, 2) + " " + Num(4 * u, 2) + "\"";
    case style::DashPattern::Dotted: return " stroke-dasharray=\"" + Num(1.5 * u, 2) + " " + Num(3 * u, 2) + "\"";
    default:
      return " stroke-dasharray=\"" + Num(6 * u, 2) + " " + Num(3 * u, 2) + " " + Num(1.5 * u, 2) + " " +
             Num(3 * u, 2) + "\"";
  }
}

std::string Tag(const draw::PaintOp& op) {
  if (op.layer.empty()) return {};
  return " data-layer=\"" + Escape(std::string(op.layer)) + "\" data-index=\"" + std::to_string(op.index) + "\"";
}

std::string Marker(const draw::PaintMarker& m, const std::string& tag, int prec) {
  const double r = std::max(m.size, 1.0) / 2;
  const std::string fill = " fill=\"" + Hex(m.color) + "\"" + Opacity("fill-opacity", m.alpha);
  const double x = m.p.x, y = m.p.y;
  auto pts = [&](std::initializer_list<std::pair<double, double>> l) {
    std::string s;
    bool first = true;
    for (const auto& [px, py] : l) {
      s += (first ? "" : " ") + Num(px, prec) + "," + Num(py, prec);
      first = false;
    }
    return s;
  };
  switch (m.shape) {
    case style::MarkerShape::Square:
      return "<rect x=\"" + Num(x - r, prec) + "\" y=\"" + Num(y - r, prec) + "\" width=\"" + Num(2 * r, prec) +
             "\" height=\"" + Num(2 * r, prec) + "\"" + fill + tag + "/>";
    case style::MarkerShape::Diamond:
      return "<polygon points=\"" + pts({{x, y - r}, {x + r, y}, {x, y + r}, {x - r, y}}) + "\"" + fill + tag + "/>";
    case style::MarkerShape::Triangle:
      return "<polygon points=\"" + pts({{x, y - r}, {x - r, y + r}, {x + r, y + r}}) + "\"" + fill + tag + "/>";
    case style::MarkerShape::Cross:
      return "<path d=\"M" + Num(x - r, prec) + "," + Num(y - r, prec) + "L" + Num(x + r, prec) + "," +
             Num(y + r, prec) + "M" + Num(x - r, prec) + "," + Num(y + r, prec) + "L" + Num(x + r, prec) + "," +
             Num(y - r, prec) + "\" fill=\"none\" stroke=\"" + Hex(m.color) + "\" stroke-width=\"" +
             Num(std::max(1.0, r / 2.5), prec) + "\"" + Opacity("stroke-opacity", m.alpha) + tag + "/>";
    default:
      return "<circle cx=\"" + Num(x, prec) + "\" cy=\"" + Num(y, prec) + "\" r=\"" + Num(r, prec) + "\"" + fill +
             tag + "/>";
  }
}

std::string Element(const draw::PaintOp& op, int prec) {
  const std::string tag = Tag(op);
  return std::visit(
      [&](const auto& sh) -> std::string {
        using T = std::decay_t<decltype(sh)>;
        if constexpr (std::is_same_v<T, draw::PaintFill>) {
          return "<rect x=\"" + Num(sh.rect.x, prec) + "\" y=\"" + Num(sh.rect.y, prec) + "\" width=\"" +
                 Num(sh.rect.w, prec) + "\" height=\"" + Num(sh.rect.h, prec) + "\" fill=\"" + Hex(sh.color) + "\"/>";
        } else if constexpr (std::is_same_v<T, draw::PaintTriangle>) {
          std::string s = "<polygon points=\"" + Points({sh.p[0], sh.p[1], sh.p[2]}, prec) + "\" fill=\"" +
                          Hex(sh.color) + "\"" + Opacity("fill-opacity", sh.alpha);
          // Opaque meshes: a hairline stroke in the fill colour hides anti-aliasing seams.
          if (sh.alpha >= 0.9995f) s += " stroke=\"" + Hex(sh.color) + "\" stroke-width=\"0.5\" stroke-linejoin=\"round\"";
          return s + tag + "/>";
        } else if constexpr (std::is_same_v<T, draw::PaintPolyline>) {
          return "<polyline points=\"" + Points(sh.p, prec) + "\" fill=\"none\" stroke=\"" + Hex(sh.color) +
                 "\" stroke-width=\"" + Num(sh.width, prec) + "\"" + Opacity("stroke-opacity", sh.alpha) +
                 Dash(sh.dash, sh.width) + " stroke-linecap=\"round\" stroke-linejoin=\"round\"" + tag + "/>";
        } else {
          return Marker(sh, tag, prec);
        }
      },
      op.shape);
}

std::string Header(const SvgOptions& o) {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" +
         std::to_string(o.width) + "\" height=\"" + std::to_string(o.height) + "\" viewBox=\"0 0 " +
         std::to_string(o.width) + " " + std::to_string(o.height) + "\">\n";
}

}  // namespace

std::string SceneGroup(const draw::SceneDrawing& scene, const draw::Rect& rect, const std::string& id,
                       const std::string& title, const SvgOptions& o) {
  const int p = o.precision;
  std::string s = "<g id=\"" + Escape(id) + "\" data-scene=\"" + Escape(scene.name) + "\" clip-path=\"url(#" +
                  Escape(id) + "-clip)\">\n";
  s += "<clipPath id=\"" + Escape(id) + "-clip\"><rect x=\"" + Num(rect.x, p) + "\" y=\"" + Num(rect.y, p) +
       "\" width=\"" + Num(rect.w, p) + "\" height=\"" + Num(rect.h, p) + "\"/></clipPath>\n";
  if (!title.empty()) s += "<title>" + Escape(title) + "</title>\n";
  for (const auto& op : draw::Paint(scene, rect)) s += Element(op, p) + "\n";
  if (o.titles && !title.empty())
    s += "<text x=\"" + Num(rect.x + 6, p) + "\" y=\"" + Num(rect.y + 16, p) +
         "\" font-family=\"sans-serif\" font-size=\"12\" fill=\"#161616\">" + Escape(title) + "</text>\n";
  return s + "</g>\n";
}

std::string SceneDocument(const draw::SceneDrawing& scene, const SvgOptions& o) {
  SvgOptions noTitle = o;
  noTitle.titles = false;
  return Header(o) +
         SceneGroup(scene, {0, 0, static_cast<double>(o.width), static_cast<double>(o.height)}, "scene", scene.name,
                    noTitle) +
         "</svg>\n";
}

std::string LayoutDocument(const std::map<std::string, draw::SceneDrawing>& scenes,
                           const std::optional<scene::layout::LayoutNode>& layout, const std::set<std::string>& open,
                           const SvgOptions& o) {
  std::string s = Header(o);
  s += "<rect x=\"0\" y=\"0\" width=\"" + std::to_string(o.width) + "\" height=\"" + std::to_string(o.height) +
       "\" fill=\"#e0e0e0\"/>\n";
  int i = 0;
  for (const auto& pane :
       draw::Arrange(layout, open, {0, 0, static_cast<double>(o.width), static_cast<double>(o.height)})) {
    auto it = scenes.find(pane.scene);
    if (it == scenes.end()) continue;
    const draw::Rect inner{pane.rect.x + 1, pane.rect.y + 1, pane.rect.w - 2, pane.rect.h - 2};
    s += SceneGroup(it->second, inner, "pane" + std::to_string(i++), pane.label, o);
  }
  return s + "</svg>\n";
}

}  // namespace fccvis::backend::svg
