# Inventory of the imported tree (T0.1)

Snapshot: the "FCC Visualization" project, imported on 2026-10-10 as commit
`Import FCCVisualization tree from project snapshot (baseline)`. This file
records what existed before any v1.0 work, how it built, and the defects found.
Later tasks reference the item numbers below.

## Modules and targets

| Dir | CMake target | Kind | Contents | Third-party deps declared |
|---|---|---|---|---|
| `src/geometry` | `fccvis_geometry` | OBJECT | `Geometry.h` (Mesh, AbstractShape, Transform, Shape, Placement), `Adapters.h` (TGeoShapeAdapter), `Detector.h` (DetectorNode, BuildNode from DD4hep) | ROOT Core/Geom, DD4hep DDCore |
| `src/filter` | `fccvis_filter` | OBJECT (empty .cc) | `Filter.h` header-only | none |
| `src/data` | `fccvis_data` | OBJECT (empty .cc) | `Data.h` (SessionData, Series, SeriesRef), `Elements.h` header-only | geometry |
| `src/scene` | `fccvis_scene` | OBJECT (empty .cc) | `Scene.h`, `Layout.h`, `Camera.h`, `Style.h`, `Mergeable.h`, `Markers.h` (legacy), `StyleTraits.h` (empty), `BaseSessionObject.h` (empty) | filter, data, geometry |
| `src/changes` | `fccvis_changes` | OBJECT (empty .cc) | `Changes.h`, `SceneChanges.h`, `CameraChanges.h`, `Revision.h`, `FootPrint.h` | filter, data, scene |
| `src/backend` | `fccvis_backend` | OBJECT (empty .cc) | `Backend.h`, `BackendRegistry.h`, `RecordingBackend.h` | data, changes, scene, impl targets |
| `src/backend/impl/fv` | `fccvis_backend_fv` | OBJECT | `FVBackend` pixel-canvas stub (draws two diagonals, writes PPM) | — |
| `src/session` | `fccvis_session` | OBJECT (empty .cc) | `Session.h`, `SessionOptions.h`; `Session.cc` is stale and not compiled | podio, EDM4HEP (required, unused) |
| `src/api` | `fccvis_api` | OBJECT (empty .cc) | `FCCVisApi.h` empty namespace | session, scene, backend |
| `src/cli` | `fccvis` executable | — | `main.cpp` prints a placeholder; `CliOptions`, `RunApplication`, `InteractiveShell` exist but are not compiled | links the shared lib |
| `src/gui-red` | `fccvis_gui` | not in build | `GuiApplication` (TApplication + TEveManager) | ROOT Eve/Gui |
| `src/visualization` | `fccvis_visualization` | not in build | `CollectionVisualizer-placeholder.h` (does not compile) | podio, EDM4hep |
| `presets/include` | — | — | `BasicFilters.h` empty | — |
| top level | `FCCVisualization` SHARED (+ optional STATIC), `fccvis_core` OBJECT | — | aggregates the OBJECT libraries | PandoraSDK, ROOT Core/RIO/Eve/Geom/RGL/EG/Gui |

## Tests

| File | Target | Needs |
|---|---|---|
| `tests/backend/fccvis_recordingbackend_tests.cc` (file header says `session_test.cpp`; this is the "session test" ARCHITECTURE refers to) | `fccvis_backend_tests` | Catch2 v3 only (header-only core) |
| `tests/backend/fccvis_fv_tests.cc` | added when `BUILD_BACKEND_FV` | Catch2 |
| `tests/geometry/test_{abstracts,shapes,transformations,placement,adapters_basics}.cc` | `fccvis_geometry_tests` | Catch2 (pure geometry) |
| `tests/geometry/test_detector_node.cc` | same | includes `Detector.h` → DD4hep |
| `tests/geometry/test_adapters_tgeo.cc` | same | ROOT Geom |
| `tests/geometry/test_detector_tree.cc`, `test_adapters_pipeline.cc` | same | DD4hep + a CLD compact file at a hard-coded `/home/kid-a/...` path |
| `tests/{api,core,filter,scene,visualization,geometry}/CMakeLists.txt` | — | all empty |

Test framework in the tree: **Catch2 v3**. Per CONVENTIONS ("GoogleTest/Catch2 only if already in the tree") it is kept.

## Docs and tooling

- `docs/`: Sphinx skeleton (MyST, Breathe, graphviz, mermaid; theme `alabaster`), pages `index`, `getting-started` (filled), `api`, `concepts`, `development` (empty). Breathe reads `docs/doxygen/xml`.
- `Doxyfile`: full Doxygen 1.18 template; imported condensed to its non-default keys (INPUT=src, XML on, EXTRACT_ALL=YES).
- `env.sh`, `test_init.cc`: the author's local environment (ROOT 6.38.00, PandoraSDK, podio, EDM4hep, DD4hep under `$HOME/scientific-software`) and a Cling script for the old shell.
- Spec kit at the root: `ARCHITECTURE.md`, `CONVENTIONS.md`, `TASKS.md`, `SPEC_*.md`, `DOCS_AND_RELEASE.md`, `STATUS.md`, `AGENT_PROMPT.md`.
- The project also contained 10 empty duplicates outside `FCCVisualization/` (`src/api/FCCVisApi.cc`, `src/*/placeholder.cc`, `src/scene/include/BaseSessionObject.h`, `src/visualization/CollectionVisualizer-placeholder.cc`); not imported.

## Baseline build and test (this environment)

Environment: Ubuntu 24.04, GCC 13.3, Clang 18, CMake 3.28, Ninja; Catch2 3.4 and Doxygen 1.9.8 installed from apt. ROOT, DD4hep, podio, EDM4hep, PandoraSDK are not installed system-wide.

| Step | Result |
|---|---|
| `cmake -S . -B build` | **fails**: `find_package(PandoraSDK REQUIRED)` (then ROOT, DD4hep, podio, EDM4HEP would also be required) |
| Recording/session test compiled directly (`g++ -std=c++20` + include dirs + Catch2) | **passes**: 7 test cases, 91 assertions |
| Pure geometry tests compiled with the non-ROOT half of `Geometry.cc` | 40 cases, **4 fail** (items 4) |
| FV backend test | would fail: "valid_scene" is never added to the dummy view (item 12) |

So the claim "core exists and its session test passes" holds for the header-only core; the build system as committed cannot configure without the full FCC stack.

## Defects and deviations found

1. Root `CMakeLists.txt` requires PandoraSDK and ROOT (Eve, Geom, RGL, EG, Gui) unconditionally; no code uses PandoraSDK. No `FCCVIS_ENABLE_<NAME>` options exist.
2. `configure_file` writes `fccvis/FccvisConfig.h` into the source tree; the committed copy holds the author's absolute paths.
3. `geometry` depends on ROOT and DD4hep (`Geometry.cc` contains the TGeo tessellation; `Adapters.h`/`Detector.h` include DD4hep/TGeo). ARCHITECTURE says geometry has no deps, and DD4hep belongs to adapter A1.
4. `Shape::Contains` divides the point by 10 (mm→cm for TGeo) inside core geometry. 4 pure geometry test cases fail because of it. Units belong to the adapter.
5. `Placement::GetTransform() const` is declared but never defined.
6. `fccvis_session` requires podio and EDM4HEP; `Session.cc` implements an older Session API (podio frames, DD4hep) and is not compiled.
7. `Session::Open` and `Session::Close` dereference `m_backends.find(id)` before checking against `end()` (undefined behaviour for an unknown id).
8. `Session::BackendIds`, `OpenScenes`, `IsRunning`, `FindScene`, `SceneNames` read shared state without the session mutex.
9. Cosmetic namespace-closing comments are wrong in `Data.h`, `Layout.h`, `Markers.h`, `Filter.h`.
10. `StyleTraits.h` is empty; the `StyleTraits` primary template and its three specializations live in `Style.h`.
11. ARCHITECTURE refers to `tests/session_test.cpp`; the file is `tests/backend/fccvis_recordingbackend_tests.cc`.
12. `FVBackend` draws fixed diagonals, ignores scene content, writes ASCII PPM; its test opens a scene that the dummy view does not contain. It is the seed of B0 (T3.1).
13. CLI `main.cpp` is a placeholder; `InteractiveShell`/`RunApplication` reference a non-existent `RootSceneManager` and the old Session API.
14. `gui-red` and `visualization` are outside the build; the visualization header has syntax errors. (Resolved: `visualization` removed in T6.1 cleanup; `gui-red` removed in T4.4, its TApplication + TEveManager start-up now lives in `src/backend/impl/root/RootRuntime`.)
15. `api` module is empty.
16. `tests/CMakeLists.txt` copies a `tests/resources` directory that does not exist.
17. Mixed language standards: `CXX_STANDARD 17` on the final libraries and `fccvis_api`, 20 elsewhere.
18. `Scene` stores its name privately with no accessor.
19. `RecordingBackend` test's last check is a tautology (`... == false || true`) with a TODO about stopped-backend id reuse.
