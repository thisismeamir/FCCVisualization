# ROOT backend (TEve)

Registry name: `root`. Built when `FCCVIS_ENABLE_ROOT` is on (default when
ROOT with the Eve, RGL and Gui libraries is found). Needs an X display at run
time; under SSH use X forwarding, on a machine without a screen `xvfb-run`.
The choice of classic TEve over the web-based REve is recorded in
[ROOT viewer experiments](../design/root-experiments.md).

The backend shows the open scenes in the ROOT Eve browser window: every
scene becomes an Eve scene, every layer an element list, and the layout
becomes Eve window packs (splits) and tabs, one GL viewer per pane, titled by
the layout's labels. You can rotate, zoom and pick in the window as in any
ROOT event display; the session follows (see *Interaction*).

```{literalinclude} ../../examples/root_session.fcv
:language: text
:start-after: "# [root]"
:end-before: "# [root]"
```

![Front and side views of the toy detector and the example event, written by `picture_dir`](../_static/root.png)

## Mapping

| fccvis | TEve |
|---|---|
| point layer | `TEvePointSet`, one per resolved (colour, size, marker) combination |
| line element | `TEveLine` (colour, width, dash pattern, opacity) |
| surface element | `TEveTriangleSet` (colour, opacity) plus a `TEveStraightLineSet` for the outline (`edgeColor`, `edgeWidth`; `wireframe` draws the outline only) |
| scene | `TEveScene` named `<backend id>: <scene>` |
| split / tabs | `TEveWindowPack` / `TEveWindowTab`; split weights are not honoured (Eve packs share space equally) |
| camera along an axis | orthographic camera looking along that axis |
| other camera direction | perspective camera (XOY floor, z up) |
| `magnification`, `position` | zoom; pan relative to the centre of the scene's bounding box |
| environment `background`, `axes` | GL clear colour; axes at the origin |

Layers are rebuilt only when their drawing changed (a filter, style or entry
change of that layer); a camera change touches only the camera.

## Interaction

Events go back to the session from a separate thread, so the session never
waits for ROOT:

| In the window | In the session |
|---|---|
| click on a point, line or surface | pick event (`Session::SetPickHandler`; the shell prints `picked in <backend>: ...`) with scene, layer, entry and element index |
| rotate or zoom a view | `ChangeCameraDirection` submitted by the backend (rejected and redrawn for scenes whose camera is fixed) |
| close the Eve window | the backend stops (`backend ls` shows it as stopped) |

Closing the Eve window ends TEve for the whole process (ROOT deletes
`gEve`); a new `root` backend can only be started after restarting the
program, and says so.

## Options

| Option | Meaning |
|---|---|
| `picture_dir=<dir>` | after every update write `<dir>/<scene>.png` (offscreen `SavePictureUsingFBO`) |
| `width`, `height` | picture size in pixels (default 800 x 600) |
| `tolerance` | relative tessellation tolerance of curved surfaces (default 0.02) |

## Threads and instances

All ROOT calls run on one process-wide thread that owns the `TApplication`
and the `TEveManager` and pumps the event loop
(`gSystem->ProcessEvents()`); several `root` backends in one session share
it and get their own window tabs. The `TApplication` is never destroyed
(ROOT 6.38 crashes when it is, see the experiments); the thread is stopped
before ROOT's own exit handlers run.

## Testing

`root::viewer`, `root::close`, `contract::root`, `matrix::root` and
`docs::root_session` run under `xvfb-run` (labels `needs-root`,
`needs-x11`). Picks, camera drags and closing are driven through the same
TEve calls a mouse action ends in (`TEveSelection::UserPickedElement`,
`TEvePointSet::PointSelected`, `TGLCamera::RotateRad`,
`TEveBrowser::CloseWindow`); real mouse input on a physical display has not
been exercised by the automated tests.
