# Layouts

A layout arranges scenes in panes: splits side by side or stacked, tabs,
labels and weights. It is shared by all backends; each shows the part made
of the scenes it has open. See {doc}`../concepts/layout` and `help layouts`.

```{literalinclude} ../../examples/guides/layouts.fcv
:language: text
:start-after: "# [split]"
:end-before: "# [split]"
```

`as "label"` titles a pane; `*2` gives a child twice the share of a split.

```{literalinclude} ../../examples/guides/layouts.fcv
:language: text
:start-after: "# [tabs]"
:end-before: "# [tabs]"
```

Tabs are alternatives: an interactive backend (ROOT) shows tabs, static
backends (raster, SVG) draw the first tab.

## What a backend shows

```{literalinclude} ../../examples/guides/layouts.fcv
:language: text
:start-after: "# [backend]"
:end-before: "# [backend]"
```

The raster backend has `xy` and `rz` open, not `side`: its `layout.png`
shows the layout pruned to the open scenes.

## Removing a scene

```{literalinclude} ../../examples/guides/layouts.fcv
:language: text
:start-after: "# [prune]"
:end-before: "# [prune]"
```

Removing a scene removes its panes; a split or tab group left with one child
is replaced by that child.

Output:

```{literalinclude} ../../examples/guides/layouts.out
:language: text
```
