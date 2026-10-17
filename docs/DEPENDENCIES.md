# Dependencies

Every dependency is recorded here with the reason it is needed and what is
disabled without it (CONVENTIONS: "Do not add dependencies without recording
them"). The core library needs nothing beyond a C++20 compiler and CMake.

## Build requirements (always)

| Dependency | Version | Why |
|---|---|---|
| C++ compiler | C++20 (tested: GCC 13.3, Clang 18) | language standard of the project |
| CMake | ≥ 3.20 | build system |

## Optional, by feature

Each adapter/backend option `FCCVIS_ENABLE_<NAME>` defaults to ON only when
its dependency is found; forcing it ON without the dependency fails at
configure time. Disabled means not compiled and not registered.

| Feature (option) | Dependency | Why | Without it |
|---|---|---|---|
| Unit tests (`FCCVIS_BUILD_TESTS`) | Catch2 v3 | test framework already used by the imported tests | tests are not built (warning) |
| Build checks (`build::module_includes`) | Python 3 | include-discipline checker `tools/check_module_includes.py` | that test is not registered |
| Detector adapter A1 (`FCCVIS_ENABLE_DD4HEP`) | DD4hep (DDCore; tested v01-38 built with DDRec, DDDetectors), ROOT (Core, Geom; tested 6.38.06) | reads compact XML / TGeo geometry; drivers are loaded through `LD_LIBRARY_PATH` | adapter not built |
| EDM4hep adapter A2 (`FCCVIS_ENABLE_EDM4HEP`) | EDM4hep (tested v01-01-01), podio with ROOT I/O (tested v01-08; built here against fmt 11.1.4 because podio's tools need `fmt::println`), ROOT (Core, RIO, Tree) | reads podio frame files; data-model dictionaries are found through `LD_LIBRARY_PATH` | adapter not built |
| CSV adapter A3 (`FCCVIS_ENABLE_CSV`) | none | | (default ON) |
| Phoenix JSON adapter A4 (`FCCVIS_ENABLE_PHOENIX_JSON`) | none (own JSON reader) | | (default ON) |
| Raster backend B0 (`FCCVIS_ENABLE_RASTER`, alias `FCCVIS_ENABLE_FV`) | none (own rasterizer and PNG writer) | | (default ON) |
| SVG backend B2 (`FCCVIS_ENABLE_SVG`) | none | | (default ON) |
| Histogram backend B3 (`FCCVIS_ENABLE_HISTOGRAM`) | none | | (default ON) |
| Shell line editing (`fccvis-shell`) | linenoise (vendored in `third_party/linenoise`, BSD-2-Clause, commit a473823) | history, completion, multi-line editing, printing asynchronous messages above the prompt; SPEC_INTERFACE names replxx/linenoise | always built (no option; no system dependency) |
| Interactive shell test (`shell::interactive`) | Python 3 (`pty`) | drives `fccvis-shell` on a pseudo-terminal | that test is not registered |
| Documentation (`FCCVIS_BUILD_DOCS`) | Doxygen ≥ 1.9 (tested 1.9.8), Python 3 with Sphinx (tested 9.1), Breathe, myst-parser, sphinxcontrib-mermaid (`docs/requirements.txt`) | API reference, coverage gate, Sphinx site | `docs` target not available |
| ROOT backend B1 (`FCCVIS_ENABLE_ROOT`) | ROOT with Eve, RGL, Gui, Ged, Geom (tested 6.38.06, built with `opengl=ON x11=ON`); an X display at run time | classic TEve viewer (decision in `docs/design/root-experiments.md`) | backend not built |
| REve web backend B1 (`FCCVIS_ENABLE_REVE`) | ROOT with `webgui` and `root7` (ROOT::ROOTEve, ROOTWebDisplay, RHTTP; tested 6.38.06), nlohmann_json (ROOT::ROOTEve links it), a browser for the user | web event display served over HTTP | backend not built |
| REve browser test (`fccvis_reve_tests`) | Node.js with the Playwright package of `tools/phoenix_verify` and its Chromium | drives the REve page | test skips |
| ROOT tests without a display (`root::*`, `contract::root`, `docs::root_session`) | `xvfb-run` (Xvfb), used through the cache variable `FCCVIS_X11_LAUNCHER` | runs the viewer on a virtual display | set `FCCVIS_X11_LAUNCHER=""` to use the current `DISPLAY` |
| Python binding (`FCCVIS_ENABLE_PYTHON`) | pybind11 (tested 2.13.6, found through `python3 -m pybind11 --cmakedir`), Python development files (tested 3.13), the shared library | module `fccvis` | binding not built |
| CED backend B4 (`FCCVIS_ENABLE_CED`) | POSIX sockets (no library; protocol implemented in `CedProtocol.h`); at run time a CED server (glced) | sends scenes to glced | default ON on UNIX |
| CED server test (`ced::glced`) | glced (CED 1.10, built with iLCUtil and freeglut; found on PATH or in `$CED_DIR/bin`), xdotool, ImageMagick `import` (screenshot), Xvfb | drives the real server | test not registered |
| Phoenix backend B5 (`FCCVIS_ENABLE_PHOENIX`) | POSIX sockets (no library) | HTTP host for Phoenix | default ON on UNIX |
| Phoenix viewer check (`phoenix::viewer`, `tools/phoenix_verify`) | Node.js (tested 22), npm packages phoenix-event-display 4.2.3, playwright 1.56.0 with its Chromium, esbuild, @angular/core (Phoenix imports it) | loads the backend's output into the Phoenix event display | test not registered |
| Phoenix format check (`phoenix::format`) | Python 3 | checks the committed golden against the documented Phoenix format | test not registered |
| Fixture generators (`tests/data/*/make_fixtures.py`) | Python 3 | regenerate committed fixtures (not run by the build) | fixtures cannot be regenerated |

Planned dependencies are added to this table by the task that introduces them.

## CTest labels

Tests that need something beyond the core carry labels so they can be
selected or excluded (`ctest -L <label>`, `ctest -LE <label>`):

| Label | Meaning |
|---|---|
| `core` | needs nothing beyond the core build |
| `build` | build-system checks (module graph, includes) |
| `needs-root` | needs ROOT libraries |
| `needs-x11` | needs an X11 display (`DISPLAY`); skips itself when none is set |
| `needs-network` | needs network access |
| `needs-<lib>` | needs third-party library `<lib>` (for example `needs-dd4hep`, `needs-podio`) |
| `needs-ced` | needs the CED display server glced |
| `needs-node` | needs Node.js with the npm packages of `tools/phoenix_verify` |
| `needs-data` | needs an external data file given by an environment variable; skips itself when unset |

Every test has exactly one of `core`, `build` or at least one `needs-*` label.
`build::labels` checks this.
