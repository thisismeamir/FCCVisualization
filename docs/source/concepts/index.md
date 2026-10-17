# Concepts

fccvis has three layers. **Adapters** turn sources into series of elements.
The **session** holds the data, the scenes (what to show and how) and the
layout, and applies every modification as a change through
`Session::Resolve`. **Backends** draw the scenes they were asked to show.
Only the session talks to backends; adapters never see scenes or backends.

```{graphviz}
digraph layers {
  rankdir=LR; bgcolor="transparent";
  node [shape=box, style="rounded,filled", fontname="Helvetica", fontsize=11, color="#525252", fillcolor="#f4f4f4"];
  edge [color="#525252", fontname="Helvetica", fontsize=9];
  files [label="files\n(CSV, DD4hep, EDM4hep)", shape=folder];
  adapters [label="Adapters"];
  session [label="Session\ndata · scenes · layout\nResolve(change)", fillcolor="#e8daff", color="#7017FF", penwidth=1.5];
  backends [label="Backends\n(raster, SVG, histogram, ROOT)"];
  user [label="C++ API · shell · scripts", shape=plaintext];
  files -> adapters -> session [label="series"];
  user -> session [label="changes"];
  session -> backends [label="sync"];
  backends -> session [label="events\n(picks, camera moves)", style=dashed];
}
```

```{toctree}
:maxdepth: 1

data
filters
styles
scenes
camera
layout
session
revisions
```

## Glossary

Series
: An ordered sequence of entries of one element type, registered by an
  adapter under a name and a source (`hits@csv:/data/hits.csv`).

Entry
: One batch of elements of a series, usually one event.

Element
: A geometry (point, polyline or surface) plus metadata attributes.

Layer
: A scene's view of one series: series reference, entry cursor, filter and
  style sheet.

Scene
: A camera, an environment and named layers. It holds no data.

Layout
: How scenes are arranged: panes, splits and tabs.

Change
: A small object describing one modification (`AddLayer`,
  `ChangeCameraDirection`, ...). Every modification is a change, applied by
  `Session::Resolve`.

Backend
: A drawing target attached to the session under an id; it shows the scenes
  opened in it.
