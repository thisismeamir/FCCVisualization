# Data and elements

The session's data (`data::SessionData`) is a typed store of **series**. A
series is addressed by element type, name and source, and is an ordered list
of **entries**; an entry is the list of elements of one event or step.

An **element** is a geometry plus **metadata**:

| Element type | Geometry |
|---|---|
| `data::PointElement` | a position |
| `data::LineElement` | an ordered list of vertices (a polyline) |
| `data::SurfaceElement` | a shape (`geometry::Shape`: an intrinsic shape and its placement) |

Metadata (`data::Metadata`) holds exactly what the source provides: integers,
numbers, flags and text under keys chosen by the adapter (`energy`,
`charge`, `subdetector`, `unit:energy`, ...). A missing key means the source
does not provide the value. Domain meaning (hit, track, cluster) lives in the
series name and description, not in the type, so backends only need to draw
three kinds of geometry. Lengths are millimetres.

```{graphviz}
digraph data {
  rankdir=LR; bgcolor="transparent";
  node [shape=record, fontname="Helvetica", fontsize=10, color="#525252", style=filled, fillcolor="#f4f4f4"];
  sd [label="{SessionData|<a>hits (point) @ csv:hits.csv|<b>tracks (line) @ csv:tracks.csv|geometry/ECal (surface) @ detector:...}"];
  series [label="{Series hits|<e0>entry 0|entry 1|entry 2}"];
  entry [label="{entry 0|{PointElement|position (1000, 0, -50)|energy 0.5 · layer 1 · detector \"ECal\"}|...}", fillcolor="#e8daff", color="#7017FF"];
  sd:a -> series; series:e0 -> entry;
}
```

Series are **lazy**: an adapter registers a provider, and an entry is
converted the first time it is needed, then kept in a small per-series cache.
Changing a filter or a style never re-reads the source. Programs that compute
their own elements register a `data::InMemorySeries<X>`. After registering
series, call `Session::DataChanged()` so that backends redraw; a growing
(live) series is announced with `Session::SeriesAppended(ref)`.
