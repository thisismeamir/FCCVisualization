/**
 * @file Drawing.h
 * @brief Helpers for software backends (raster, SVG, histogram): turn a scene
 *        into styled world-space primitives, keep them per layer, project them.
 *
 * This is backend-side code (ARCHITECTURE decision 8: each backend
 * materializes scenes itself; nothing here is exchanged with the session).
 * Library backends (ROOT, Phoenix) do not need it.
 *
 * Retained drawing: RetainedScenes applies a SyncPlan (RevisionTracker) and
 * rebuilds only the layers the plan names; camera and environment changes
 * re-project without rebuilding layers. Built layers are immutable and shared,
 * so a render thread can use a snapshot while the session keeps going.
 *
 * Projection: orthographic along the camera direction. Screen "up" is world +y
 * (or +z when looking along y). The picture is centred on the content's
 * bounding box and scaled to fit; the camera magnification zooms around the
 * centre and the camera position pans (its projection on the image plane).
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include "Backend.h"
#include "Layout.h"
#include "RevisionTracker.h"
#include "Scene.h"

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace fccvis::backend::draw {

/** @brief World-space point in millimetres. */
using Vec3 = std::array<double, 3>;

/** @brief A styled point. */
struct PointPrim {
  Vec3 p;                 ///< Position.
  style::PointStyle s;    ///< Completed style.
  std::size_t index = 0;  ///< Index of the element in its entry.
};

/** @brief A styled polyline. */
struct LinePrim {
  std::vector<Vec3> v;    ///< Vertices.
  style::LineStyle s;     ///< Completed style.
  std::size_t index = 0;  ///< Index of the element in its entry.
};

/** @brief A styled triangle mesh in world coordinates. */
struct MeshPrim {
  std::vector<Vec3> vertices;                          ///< World positions.
  std::vector<std::array<std::uint32_t, 3>> triangles; ///< Indices into vertices.
  std::vector<std::array<std::uint32_t, 2>> edges;     ///< Outline: boundary edges and creases (see FeatureEdges).
  style::SurfaceStyle s;                               ///< Completed style.
  std::size_t index = 0;                               ///< Index of the element in its entry.
};

/** @brief Built content of one layer. Immutable once built. */
struct LayerDrawing {
  std::string name;              ///< Layer name.
  std::vector<PointPrim> points; ///< Points, in entry order.
  std::vector<LinePrim> lines;   ///< Lines, in entry order.
  std::vector<MeshPrim> meshes;  ///< Surfaces, in entry order.
};

/** @brief Lowest-priority styles a backend completes every element with. */
struct Defaults {
  style::PointStyle point;          ///< Point defaults.
  style::LineStyle line;            ///< Line defaults.
  style::SurfaceStyle surface;      ///< Surface defaults.
  style::EnvironmentStyle environment;  ///< Environment defaults.
};

/** @brief Defaults of the figure theme (monochrome, accent #7017FF, white background). */
[[nodiscard]] Defaults ThemeDefaults();

/** @brief Camera state copied from a scene. */
struct CameraState {
  Vec3 position{0, 0, 0};    ///< Pan (projected onto the image plane).
  Vec3 direction{0, 0, 1};   ///< Viewing direction.
  double magnification = 1;  ///< Zoom factor.
};

/** @brief What a backend draws for one scene. */
struct SceneDrawing {
  std::string name;                       ///< Scene name.
  CameraState camera;                     ///< Camera snapshot.
  style::EnvironmentStyle environment;    ///< Completed environment.
  std::map<std::string, std::shared_ptr<const LayerDrawing>> layers;  ///< By layer name.
};

/**
 * @brief Builds one layer: selects and styles its elements (Scene::Prepare),
 *        tessellates surfaces into world meshes.
 * @param scene     Scene the layer belongs to.
 * @param data      Session data (the layer's series).
 * @param layer     Layer name.
 * @param defaults  Styles completing every element.
 * @param tolerance Relative tessellation tolerance for surfaces.
 * @param kinds     Element kinds the backend draws; other kinds are skipped.
 */
[[nodiscard]] std::shared_ptr<const LayerDrawing> BuildLayer(const scene::Scene& scene,
                                                             const data::SessionData& data,
                                                             const std::string& layer,
                                                             const Defaults& defaults, double tolerance,
                                                             const std::set<ElementKind>& kinds);

/** @brief Per-scene, per-layer retained drawings, updated from SyncPlans. */
class RetainedScenes {
 public:
  /**
   * @param defaults  Styles completing every element.
   * @param tolerance Tessellation tolerance for surfaces.
   * @param kinds     Element kinds drawn.
   */
  RetainedScenes(Defaults defaults, double tolerance, std::set<ElementKind> kinds)
      : m_defaults(std::move(defaults)), m_tolerance(tolerance), m_kinds(std::move(kinds)) {}

  /** @brief Rebuilds what @p plan names; drops closed scenes. */
  void Apply(const SyncPlan& plan, const SessionView& view);

  /** @brief Current drawings (cheap copy: layers are shared). */
  [[nodiscard]] std::map<std::string, SceneDrawing> Snapshot() const { return m_scenes; }

  /** @brief Forgets a scene. */
  void Forget(const std::string& scene) { m_scenes.erase(scene); }

 private:
  Defaults m_defaults;
  double m_tolerance;
  std::set<ElementKind> m_kinds;
  std::map<std::string, SceneDrawing> m_scenes;
};

/** @brief Screen-space position: x right, y down (pixels), depth (larger is nearer). */
struct Projected {
  double x = 0;      ///< Horizontal position (pixels, right).
  double y = 0;      ///< Vertical position (pixels, down).
  double depth = 0;  ///< Depth along the viewing direction (larger is nearer).
};

/** @brief Orthographic projection of one scene into a rectangle. */
class View {
 public:
  /**
   * @brief Fits the drawing's content into a rectangle.
   * @param d       Scene drawing (content bounds and camera).
   * @param x0, y0  Top-left corner of the rectangle (pixels).
   * @param w, h    Size of the rectangle (pixels).
   * @param margin  Fraction of the smaller side kept free around the content.
   */
  View(const SceneDrawing& d, double x0, double y0, double w, double h, double margin = 0.05);

  /** @brief Projects a world point. */
  [[nodiscard]] Projected Project(const Vec3& p) const;
  /** @brief Pixels per millimetre. */
  [[nodiscard]] double Scale() const { return m_scale; }
  /** @brief Screen right/up/forward basis in world coordinates. */
  [[nodiscard]] const std::array<Vec3, 3>& Basis() const { return m_basis; }

 private:
  std::array<Vec3, 3> m_basis{};  // right, up, forward
  double m_scale = 1, m_cx = 0, m_cy = 0, m_ox = 0, m_oy = 0;
};

/** @brief A rectangle in pixels. */
struct Rect {
  double x = 0;  ///< Left edge.
  double y = 0;  ///< Top edge.
  double w = 0;  ///< Width.
  double h = 0;  ///< Height.
};

/** @brief A pane of a laid-out picture: scene, rectangle and display label. */
struct PaneRect {
  std::string scene;  ///< Scene shown.
  Rect rect;          ///< Where (pixels).
  std::string label;  ///< Label from the layout, or the scene name.
};

/**
 * @brief Places scenes in a rectangle following a (pruned) layout.
 *
 * Splits divide the rectangle by weight. Tabs show their first child only
 * (static outputs cannot switch tabs); the other tabs are listed in
 * @p hiddenTabs. Without a layout the open scenes are tiled in a near-square
 * grid, in name order.
 */
[[nodiscard]] std::vector<PaneRect> Arrange(const std::optional<scene::layout::LayoutNode>& layout,
                                            const std::set<std::string>& open, Rect area,
                                            std::vector<std::string>* hiddenTabs = nullptr);

/** @brief Effective opacity of a style: colour alpha times opacity, in [0, 1]. */
[[nodiscard]] float Alpha(const style::CommonStyle& c);
/** @brief False when the style hides the element. */
[[nodiscard]] bool Visible(const style::CommonStyle& c);
/** @brief Colour of a style (black if unset). */
[[nodiscard]] style::Color ColorOf(const style::CommonStyle& c);
/** @brief Smallest of 1, 2, 5 x 10^n that is >= @p raw (grid spacing). */
[[nodiscard]] double NiceStep(double raw);
/**
 * @brief Outline edges of a mesh: boundary edges and creases (adjacent faces
 *        meeting at more than 20 degrees). BuildLayer stores them in MeshPrim::edges.
 */
[[nodiscard]] std::vector<std::array<std::uint32_t, 2>> FeatureEdges(const MeshPrim& m);

/** @brief Number with at most @p precision decimals, trailing zeros removed, no "-0" (stable text output). */
[[nodiscard]] std::string FormatNumber(double v, int precision);
/** @brief Escapes &, <, >, " and ' for XML text and attributes. */
[[nodiscard]] std::string XmlEscape(const std::string& s);

/** @brief Paint operation: filled rectangle. */
struct PaintFill {
  Rect rect;          ///< Rectangle.
  style::Color color; ///< Fill colour.
};
/** @brief Paint operation: filled triangle. */
struct PaintTriangle {
  std::array<Projected, 3> p;  ///< Corners (screen).
  style::Color color;          ///< Fill colour.
  float alpha = 1;             ///< Opacity.
};
/** @brief Paint operation: stroked polyline. */
struct PaintPolyline {
  std::vector<Projected> p;                              ///< Vertices (screen).
  double width = 1;                                      ///< Stroke width (pixels).
  style::Color color;                                    ///< Stroke colour.
  float alpha = 1;                                       ///< Opacity.
  style::DashPattern dash = style::DashPattern::Solid;   ///< Dash pattern.
};
/** @brief Paint operation: point marker. */
struct PaintMarker {
  Projected p;                                           ///< Centre (screen).
  double size = 3;                                       ///< Diameter (pixels).
  style::MarkerShape shape = style::MarkerShape::Circle; ///< Marker shape.
  style::Color color;                                    ///< Fill colour.
  float alpha = 1;                                       ///< Opacity.
};

/** @brief What a paint operation draws, for grouping and attributes in outputs. */
enum class PaintRole { Background, Grid, Surface, Edge, Line, Point, Axis };

/** @brief One operation of a painter's-ordered display list. */
struct PaintOp {
  PaintRole role = PaintRole::Background;  ///< What it is.
  std::string_view layer;                  ///< Layer name (empty for environment); valid while the SceneDrawing lives.
  std::size_t index = 0;                   ///< Element index in its entry.
  std::variant<PaintFill, PaintTriangle, PaintPolyline, PaintMarker> shape;  ///< Geometry and paint.
};

/**
 * @brief Painter's-ordered display list of one scene in a rectangle.
 *
 * Order: background, grid (environment), surface triangles far to near,
 * surface outlines, polylines far to near, points far to near, axes. Shared
 * by the raster and SVG backends so both draw the same picture.
 */
[[nodiscard]] std::vector<PaintOp> Paint(const SceneDrawing& scene, const Rect& rect);

}  // namespace fccvis::backend::draw
