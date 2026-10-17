# Filters

A filter (`filter::Filter<X>`) decides whether an element passes. Filters
never modify or reorder data; a layer's filter selects what the layer shows,
and a style rule's filter selects which elements the rule styles. Filters
compose with `&`, `|` and `!`, and positional filters see the element's
index in its entry (`first(5)`).

```{graphviz}
digraph filters {
  rankdir=LR; bgcolor="transparent";
  node [shape=box, style="rounded,filled", fontname="Helvetica", fontsize=10, color="#525252", fillcolor="#f4f4f4"];
  edge [color="#525252", fontname="Helvetica", fontsize=9];
  entry [label="entry\n(all elements)"];
  f [label="layer filter\nenergy > 1 and detector == \"ECal\"", fillcolor="#e8daff", color="#7017FF"];
  shown [label="elements shown"];
  entry -> f -> shown;
}
```

The standard library builds filters from attributes, energy, shapes and
detector structure (`stdlib::EnergyAbove`, `stdlib::Inside`,
`stdlib::Subdetector`, ...); its `Selector` works for any element type. The
shell and configuration files write the same filters as expressions:
`energy > 0.5 and inside(cylinder(0, 2000, 3000))`. A missing attribute fails
every comparison, so `charge != 1` does not select elements without charge.
See [standard filters](../stdlib/filters.md) and
[filter expressions](../shell/index.md#filter-expressions).
