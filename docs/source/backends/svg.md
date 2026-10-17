# SVG backend

Registry name: `svg`. Built when `FCCVIS_ENABLE_SVG` is on (default; no
dependencies). Writes vector pictures for papers and slides. It draws exactly
what the [raster backend](raster.md) draws (same projection, same painter's
order, same theme), as SVG elements.

```{literalinclude} ../../examples/backend_svg.cpp
:language: cpp
:start-after: "// [svg]"
:end-before: "// [svg]"
```

## Outputs

| File | Content |
|---|---|
| `<output_dir>/<scene>.svg` | one picture per open scene |
| `<output_dir>/layout.svg` | the layout: each pane is a `<g>` group clipped to its rectangle and titled with its layout label (or the scene name); tabs show their first tab |

Elements carry `data-layer` and `data-index` attributes (layer name and
element index in the entry), so a picture can be post-processed or made
interactive by other tools.

## Options and capabilities

Generated from the backend's capabilities: see [backend reference](../reference/backends.md#svg).
The shell completes the option names (`backend init svg <Tab>`).

## Mapping

| Element | SVG |
|---|---|
| point | `circle`, `rect` (square), `polygon` (diamond, triangle) or `path` (cross) |
| line | `polyline` with `stroke-width`, `stroke-dasharray` for dash patterns |
| surface | `polygon` per triangle, far to near, then outline `polyline`s (boundary and crease edges); `wireframe` draws outlines only |
| environment | background `rect`, grid lines, axes (x red, y green, z blue) |

Output is stable text: the tests compare it with `tests/data/golden/svg_layout.svg`.

## Degradations

Declared in `Capabilities`: no interaction or picking, no perspective camera
(drawn orthographically), no per-vertex shading, tabs show their first tab.
