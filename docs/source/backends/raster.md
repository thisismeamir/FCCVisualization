# Raster backend (PNG)

Registry names: `raster` (and `fv`, its former name). Built when
`FCCVIS_ENABLE_RASTER` is on (default; `FCCVIS_ENABLE_FV` is accepted as an
alias). No external dependency: the rasterizer and the PNG writer are part of
fccvis.

The raster backend draws an orthographic projection of every open scene:
points as markers, lines as polylines (width and dash pattern), surfaces as
filled triangles with outline edges (or outline only with `wireframe`). Styles
are the resolved styles of the session, completed by the figure theme
(monochrome, accent `#7017FF`, white background).

```{literalinclude} ../../examples/backend_raster.cpp
:language: cpp
:start-after: "// [raster]"
:end-before: "// [raster]"
```

## Outputs

After each synchronisation that changed something, on the backend's own
render thread (call `Session::Flush` to wait):

| File | Content |
|---|---|
| `<output_dir>/<scene>.png` | one image per open scene |
| `<output_dir>/layout.png` | the layout, when one is set: splits by weight, tabs show their first tab |
| `output=<file>` (option) | legacy single picture; `.ppm` writes ASCII PPM |

Bursts of changes are coalesced: only the latest state is rendered.

## Options and capabilities

Generated from the backend's capabilities: see [backend reference](../reference/backends.md#raster).
The shell completes the option names (`backend init raster <Tab>`).

## Projection

Orthographic along the camera direction. Screen up is world +y (+z when looking
along y); screen right completes a right-handed frame, so with the default
direction +z, world +x points to the left. The picture is fitted to the
content's bounding box; the camera magnification zooms around the centre and
the camera position pans.

## Degradations

Declared in `Capabilities`: no interaction or picking, no text (labels are not
drawn), tabs show their first tab, perspective cameras are drawn
orthographically. Element kinds other than points, lines and surfaces are
reported once per layer by the session and skipped.

## Visual regression tests

Tests compare rendered images with goldens in `tests/data/golden/` using
`tests/support/GoldenImage.h` (per-channel tolerance, maximum fraction of
differing pixels). On a mismatch the actual image and a diff image are written
to `<build>/tests/golden-out/`. To accept an intended change, rerun with
`FCCVIS_UPDATE_GOLDEN=1` and review the new goldens before committing.
