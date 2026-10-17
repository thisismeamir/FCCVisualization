# Scenes and layers

A scene is the unit of visualization. It holds no data: it has a camera
(possibly shared with other scenes), camera options, an environment and named
**layers**. A layer points at a series, has an entry cursor (which event),
a filter and a style sheet.

```{graphviz}
digraph scene {
  rankdir=TB; bgcolor="transparent";
  node [shape=box, style="rounded,filled", fontname="Helvetica", fontsize=10, color="#525252", fillcolor="#f4f4f4"];
  edge [color="#525252"];
  scene [label="scene \"front\"", fillcolor="#e8daff", color="#7017FF"];
  cam [label="camera\n(shared handle)"]; env [label="environment"];
  l1 [label="layer hits\nseries hits · entry 2\nfilter · style sheet"];
  l2 [label="layer trk\nseries tracks · entry 2\nfilter · style sheet"];
  scene -> cam; scene -> env; scene -> l1; scene -> l2;
}
```

Two layers of the same series (or of the same element type) have independent
filters and styles, which is how reconstructed and simulated tracks, or
selected and ghosted hits, are shown together. A scene may appear in several
panes and several backends; they then share its view state.

Layers are added with `AddLayer<X>` (`layer add` in the shell), moved to
another event with `SetEntry<X>` (`entry`), selected with `SetLayerFilter<X>`
(`where`) and styled with `AddStyleRule<X>` (`style`).
