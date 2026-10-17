# Layout and pruning

The session has one layout: a tree of **panes** (a scene), **splits**
(children side by side or stacked, by integer weight) and **tabs**
(alternatives). Children may carry labels, which backends show as titles.

Each backend shows only the scenes opened in it, so it receives the layout
**pruned** to those scenes: a pane survives if its scene is open; splits and
tabs drop closed children, disappear when empty and collapse into their only
remaining child; weights are kept among the survivors.

```{graphviz}
digraph prune {
  rankdir=LR; bgcolor="transparent"; compound=true;
  node [shape=box, style="rounded,filled", fontname="Helvetica", fontsize=10, color="#525252", fillcolor="#f4f4f4"];
  edge [color="#525252", fontname="Helvetica", fontsize=9];
  subgraph cluster_full { label="session layout"; fontname="Helvetica"; fontsize=10; color="#c6c6c6";
    s1 [label="split"]; xy [label="xy *2"]; t [label="tabs"]; rz [label="rz"]; xz [label="xz"];
    s1 -> xy; s1 -> t; t -> rz; t -> xz; }
  subgraph cluster_pruned { label="open in backend: xy, rz"; fontname="Helvetica"; fontsize=10; color="#7017FF";
    s2 [label="split", fillcolor="#e8daff", color="#7017FF"]; xy2 [label="xy *2"]; rz2 [label="rz"];
    s2 -> xy2; s2 -> rz2; }
  xz -> s2 [ltail=cluster_full, lhead=cluster_pruned, label="prune"];
}
```

Static backends (raster, SVG) cannot switch tabs; they show the first tab and
say so in their capabilities. Layout expressions in the shell:
`split(xy as "Front" *2, tabs(rz, xz))`.
