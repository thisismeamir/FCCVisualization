# Camera

A camera has a position, a viewing direction and a magnification. The
software backends draw an orthographic projection along the direction, with
screen "up" along +y (+z when looking along y), fitted to the content; the
magnification zooms around the centre and the position pans.

```{graphviz}
digraph camera {
  rankdir=LR; bgcolor="transparent";
  node [shape=box, style="rounded,filled", fontname="Helvetica", fontsize=10, color="#525252", fillcolor="#f4f4f4"];
  edge [color="#525252", fontname="Helvetica", fontsize=9];
  cam [label="camera\nposition · direction · magnification", fillcolor="#e8daff", color="#7017FF"];
  a [label="scene xy"]; b [label="scene xy-zoom"];
  user [label="user / script", shape=plaintext]; viewer [label="viewer (backend)", shape=plaintext];
  a -> cam; b -> cam [label="shared"];
  user -> cam [label="ChangeCamera*"];
  viewer -> cam [label="ChangeCamera* (origin = backend)", style=dashed];
}
```

Camera options set the **mode**: `Free` (default), `Fixed` (changes that a
backend submits, for example from mouse interaction, are rejected, and the
backend is told to redraw the session's camera; user and script changes still
apply) or `Animated`. Scenes may share one camera handle (`camera <scene>
share <other>`), so moving one moves both.
