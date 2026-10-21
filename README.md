# FCCVisualization

> This is not an official webpage and is a personal development for the while. Do not assume that this is related and validated in CERN FCC project.

FCCVisualization (fccvis) is a toolkit for event and detector displays. It
reads detector geometry and event data, lets you describe what to show as
**scenes** (layers of data with filters and styles, a camera, a layout), and
draws them with interchangeable **backends**: PNG and SVG pictures,
histograms of the selected elements, the ROOT event displays (TEve and the
web-based REve), Phoenix and CED.

It started as a generalisation of PandoraMonitoring: instead of one more
hard-coded view for every need, a visualisation is described as data and
changed through one catalogue of changes, so new views need configuration
rather than code. It does not depend on PandoraSDK.

```
Adapters (source -> series)  ->  Session (data, scenes, layout, changes)  ->  Backends (draw)
```

Version 1.0.0 (release candidate; see [CHANGELOG.md](CHANGELOG.md)).

## Components

| Kind | Name | Needs |
|---|---|---|
| Adapter | `csv` — points and polylines from delimited text | — |
| Adapter | `phoenix` — Phoenix event-data JSON | — |
| Adapter | `detector` — DD4hep compact XML, TGeo ROOT/GDML geometry | DD4hep, ROOT |
| Adapter | `edm4hep` — EDM4hep (podio) event files | EDM4hep, podio, ROOT |
| Backend | `raster` — PNG pictures | — |
| Backend | `svg` — SVG pictures | — |
| Backend | `histogram` — distributions of the selected elements (CSV, SVG) | — |
| Backend | `phoenix` — Phoenix event JSON, OBJ geometry, HTTP host | POSIX |
| Backend | `ced` — the CED display server (glced) | POSIX, glced at run time |
| Backend | `root` — ROOT classic TEve viewer | ROOT (Eve, RGL), X display |
| Backend | `reve` — ROOT REve web viewer | ROOT (webgui, root7) |
| Interface | C++ API (builders, scripts, undo), `fccvis-shell`, Python module `fccvis` | Python binding: pybind11 |

Each optional part has a CMake option `FCCVIS_ENABLE_<NAME>` that is ON when
its dependency is found. Tested versions and CTest labels:
[docs/DEPENDENCIES.md](docs/DEPENDENCIES.md).

## Build

```sh
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="/path/to/root;/path/to/dd4hep;/path/to/edm4hep"
cmake --build build
ctest --test-dir build            # -L core for the tests without third-party packages
```

Requirements: CMake 3.20, a C++20 compiler (tested with GCC 13), Catch2 v3
for the tests. Documentation: `-DFCCVIS_BUILD_DOCS=ON` and
`cmake --build build --target docs` (Doxygen, Sphinx; see
`docs/requirements.txt`).

## A first picture

```text
$ build/bin/fccvis-shell
fccvis> load csv docs/examples/data/hits.csv entry=event
fccvis> scene new event
fccvis> layer add event hits
fccvis> style event/hits preset hits-by-energy 0 3
fccvis> backend init raster png output_dir=out
fccvis> open png event
fccvis> backend flush
```

`out/event.png` now shows the hits of event 0; `entry event/hits next` moves
to the next event, `where event/hits energy > 1` selects, `help` lists every
command. The same in C++ and Python, and much more, is in the documentation
(start with *Getting started*).

## Documentation

`docs/source` (Sphinx): introduction, quickstart, concepts, guides, adapter
and backend reference, shell reference, C++ and Python API, extending
fccvis, design notes and known limitations.

## License

FCCVisualization is distributed under the **Apache License, Version 2.0**.

Copyright © 2026 FCC CERN / FCCVisualization contributors.

See [`LICENSE`](LICENSE) for the complete license text.

## Maintainer

**Primary developer:** Amir Ebrahimnezhad — [amir.ebh@cern.ch](mailto:amir.ebh@cern.ch)
