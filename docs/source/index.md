# FCC Visualization

> This is not an official webpage and is a personal development for the while. Do not assume that this is related and validated in CERN FCC project.

FCC Visualization (fccvis) is a toolkit for event and detector displays: it
reads detector geometry and event data, lets you describe what to show in
**scenes** (layers of data with filters and styles, a camera, a layout), and
draws them with interchangeable **backends**: PNG and SVG pictures, histograms
of the selected elements, and interactive viewers.

It started as a generalisation of PandoraMonitoring, the hard-coded display of
the Pandora particle-flow software: instead of adding one more hard-coded
feature for every new need, fccvis describes visualisations as data and
changes, so that new views need configuration rather than new code. It does
not depend on PandoraSDK. The code is at
`https://github.com/thisismeamir/FCCVisualization`.

- {doc}`Getting started <getting-started/index>`: build, load a file, see a picture.
- {doc}`Concepts <concepts/index>`: the three layers, scenes, filters, styles, layout, revisions.
- {doc}`Shell <shell/index>`, {doc}`guides <guides/index>`, {doc}`C++ API <api/index>`.
- {doc}`Extending <development/index>`: writing adapters and backends.

```{toctree}
:maxdepth: 2
:caption: Using fccvis

getting-started/index
concepts/index
guides/index
shell/index
```

```{toctree}
:maxdepth: 2
:caption: Reference

adapters/index
backends/index
stdlib/index
api/index
reference/index
```

```{toctree}
:maxdepth: 2
:caption: Project

development/index
design/index
release/index
```
