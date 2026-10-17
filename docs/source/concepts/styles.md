# Styles and the cascade

A style is a record of optional fields: `PointStyle` (colour, opacity,
visibility, size, marker), `LineStyle` (width, dash), `SurfaceStyle` (edge
colour and width, wireframe) and `EnvironmentStyle` (background, axes, grid).
Fields left unset mean "no opinion".

Each layer has a style sheet: a base style and an ordered list of **rules**,
each pairing a filter with a partial style. Resolving an element starts from
the base, applies every rule whose filter passes in order (later rules win
for the fields they set), and the backend completes what is still unset with
its own defaults (the figure theme: monochrome with accent `#7017FF`).

```{graphviz}
digraph cascade {
  rankdir=LR; bgcolor="transparent";
  node [shape=box, style="rounded,filled", fontname="Helvetica", fontsize=10, color="#525252", fillcolor="#f4f4f4"];
  edge [color="#525252", fontname="Helvetica", fontsize=9];
  base [label="base\nsize 3"];
  r1 [label="rule: energy > 1\ncolor #7017FF"];
  r2 [label="rule: layer >= 3\nmarker square"];
  defaults [label="backend defaults\nmarker circle, opacity 1"];
  resolved [label="resolved style", fillcolor="#e8daff", color="#7017FF"];
  base -> r1 [label="then"]; r1 -> r2 [label="then"]; r2 -> resolved; defaults -> resolved [label="CompleteBy", style=dashed];
}
```

Backends only see resolved styles; a backend that cannot draw a property
(dash patterns, say) says so in its capabilities. The standard library
provides palettes, attribute-driven rules and presets; see
[styles](../stdlib/styles.md).
