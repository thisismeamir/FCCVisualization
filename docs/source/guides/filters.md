# Filters by energy, sub-detector and shape

A layer's filter decides which elements of its series it shows. The same
selection can be written as a shell expression (`where`) or with the C++
standard library ({doc}`../stdlib/filters`). Reference: `help filters` and
{doc}`../shell/index`.

## In the shell

```{literalinclude} ../../examples/guides/filters.fcv
:language: text
:start-after: "# [energy]"
:end-before: "# [energy]"
```

```{literalinclude} ../../examples/guides/filters.fcv
:language: text
:start-after: "# [subdetector]"
:end-before: "# [subdetector]"
```

`~` matches a regular expression against the whole text; `in [...]` tests
membership. An element without the attribute never passes a comparison (not
even `!=`); use `exists(attr)` to test for it.

```{literalinclude} ../../examples/guides/filters.fcv
:language: text
:start-after: "# [shape]"
:end-before: "# [shape]"
```

Shapes are in mm and centred at the origin unless translated:
`cylinder(rmin, rmax, halfz)`, `box(dx, dy, dz)` with half-lengths. A point
is tested at its position; a line by any vertex by default (`all`,
`centroid` as second argument); a surface at the centre of its bounding box.

To see how many elements each layer selects, attach the histogram backend:
its CSV output has one histogram per layer and attribute, with the number of
entries.

```{literalinclude} ../../examples/guides/filters.fcv
:language: text
:start-after: "# [count]"
:end-before: "# [count]"
```

## In C++

The same recipes with `fccvis::stdlib`; the example checks that each C++
selector and its shell expression select the same hits of the example event:

```{literalinclude} ../../examples/guide_filters.cpp
:language: cpp
:start-after: "// [recipes]"
:end-before: "// [recipes]"
```

Selectors compose with `&`, `|` and `!`, convert to a `Filter<X>` of any
element kind, and can be combined with a hand-written `Filter<X>`.
