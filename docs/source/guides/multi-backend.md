# Multi-backend sessions

One session can drive several backends at once: a picture for a report, a
vector version for a paper, histograms of the same selection and an
interactive viewer, all following the same changes. Each backend draws only
what changed since its last update.

```{literalinclude} ../../examples/guide_multi_backend.cpp
:language: cpp
:start-after: "// [backends]"
:end-before: "// [backends]"
```

The example prints (and checks the redraw counts):

```text
png    tabs: no (first tab), style properties: 13
vector tabs: no (first tab), style properties: 13
stats  tabs: no (first tab), style properties: 0
png    redrew hits 2 times, tracks 1 times
vector redrew hits 2 times, tracks 1 times
```

Switching the entry of the `hits` layer redrew that layer only; the tracks
were drawn once. Backends declare their capabilities (element kinds, style
properties, splits, tabs, options); the session warns once when a scene
contains a layer a backend cannot draw, and the backend leaves it out.

Every backend works on its own thread; `Session::Flush()` (shell:
`backend flush`) waits for all of them, for example before reading their
files.

```{literalinclude} ../../examples/guide_multi_backend.cpp
:language: cpp
:start-after: "// [detach]"
:end-before: "// [detach]"
```

Each backend has its own set of open scenes, so the same layout can be shown
completely in one backend and partly in another. Detaching a backend does
not affect the others.

In the shell (`backend ls` lists each backend's open scenes):

```{literalinclude} ../../examples/guides/multi_backend.fcv
:language: text
:start-after: "# [shell]"
:end-before: "# [shell]"
```

```{literalinclude} ../../examples/guides/multi_backend.out
:language: text
:start-after: "unit:energy]"
:end-before: "--- stderr"
```
