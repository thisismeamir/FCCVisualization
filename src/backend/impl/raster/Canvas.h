/**
 * @file Canvas.h
 * @brief Software rasterizer used by the raster backend (no anti-aliasing).
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include "Drawing.h"
#include "Png.h"
#include "Style.h"

namespace fccvis::backend::raster {

/** @brief RGBA canvas with alpha blending and a clip rectangle. */
class Canvas {
 public:
  /** @brief Canvas of @p w x @p h pixels filled with @p background. */
  Canvas(int w, int h, style::Color background);

  /** @brief Restricts drawing to a rectangle (pixels). */
  void Clip(const draw::Rect& r);
  /** @brief Fills a rectangle (respects the clip). */
  void FillRect(const draw::Rect& r, style::Color c, float alpha = 1.f);
  /** @brief Draws a point marker of diameter @p size centred at (x, y). */
  void Marker(style::MarkerShape shape, double x, double y, double size, style::Color c, float alpha);
  /**
   * @brief Draws a line segment of @p width pixels with a dash pattern.
   * @param phase Dash phase in pixels, carried along a polyline.
   */
  void Line(double x0, double y0, double x1, double y1, double width, style::Color c, float alpha,
            style::DashPattern dash, double& phase);
  /** @brief Fills a triangle. */
  void Triangle(double x0, double y0, double x1, double y1, double x2, double y2, style::Color c, float alpha);
  /** @brief The image. */
  [[nodiscard]] const Image& Pixels() const { return m_img; }

 private:
  void Blend(int x, int y, style::Color c, float alpha);
  void Solid(double x0, double y0, double x1, double y1, double width, style::Color c, float alpha);
  Image m_img;
  int m_cx0 = 0, m_cy0 = 0, m_cx1 = 0, m_cy1 = 0;  // clip, exclusive max
};

/**
 * @brief Renders one scene drawing into a rectangle of the canvas: background,
 *        grid and axes (environment), surfaces, lines, points, depth-sorted.
 */
void RenderScene(Canvas& canvas, const draw::SceneDrawing& scene, const draw::Rect& rect);

}  // namespace fccvis::backend::raster
