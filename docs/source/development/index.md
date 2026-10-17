# Extending fccvis

fccvis grows by adding **adapters** (new input formats) and **backends**
(new outputs). Neither needs a change to the core: both implement a small
interface, register themselves by name, and are then usable from C++, the
shell (`load <adapter> ...`, `backend init <type> ...`), scripts and saved
configurations. This page shows a minimal one of each (both compiled and run
by the test suite) and lists what a component must do to be accepted.

## Where code goes

The code base is a set of modules (one OBJECT library each, see
`cmake/FCCVisModules.cmake`) with an enforced dependency graph:

| Kind | Directory | May depend on | Third-party libraries |
|---|---|---|---|
| adapter | `src/adapters/<name>/` | `adapters`, `data` (geometry types come with `data`) | yes, its own |
| backend | `src/backend/impl/<name>/` | `backend` (and `stdlib`) | yes, its own |
| core modules | `src/<module>/` | the modules below them | no |

An adapter never sees scenes or backends; a backend never sees adapters.
Configuring fails on a dependency the table does not allow
(`build::module_graph_*` tests), and `build::module_includes` checks that a
module includes only headers of the modules it declares.

A new module is declared like this (the EDM4hep adapter, `src/adapters/edm4hep/CMakeLists.txt`):

```{literalinclude} ../../../src/adapters/edm4hep/CMakeLists.txt
:language: cmake
```

and enabled by an option that defaults to ON only when its dependency is
found (top-level `CMakeLists.txt`):

```{literalinclude} ../../../CMakeLists.txt
:language: cmake
:start-at: fccvis_dependent_option(FCCVIS_ENABLE_EDM4HEP
:end-at: were not found")
```

plus `add_subdirectory` in `src/adapters/CMakeLists.txt` (or
`src/backend/impl/CMakeLists.txt`) under that option, and a row in
`docs/DEPENDENCIES.md`.

## Writing an adapter

An adapter implements `fccvis::adapters::Adapter` (`Adapter.h`): a name,
its capabilities, and `Open`, which registers series in the session's data.
A complete one, for a whitespace-separated text format:

```{literalinclude} ../../examples/extend_adapter.cpp
:language: cpp
:start-after: "// [adapter]"
:end-before: "// [adapter]"
```

Using it is no different from a built-in adapter:

```{literalinclude} ../../examples/extend_adapter.cpp
:language: cpp
:start-after: "// [use]"
:end-before: "// [use]"
```

The rules an adapter follows:

- **Report, do not throw.** Bad content (unreadable file, malformed rows,
  unsupported collections) goes into `LoadReport::skipped` with a reason;
  only invalid *options* throw `std::invalid_argument`. A report with no
  series makes the shell's `load` fail with the reasons.
- **Source label** `"<adapter>:<absolute path>"`, so that the same series
  name from two files stays apart and a second `load` of the same file
  finds the existing series (`RegisteredSeries::existing`) instead of
  loading again.
- **Lazy series.** `Series<X>::Load(entry)` is called when an entry is first
  needed; the session caches converted entries (bounded, per series).
  Calls to one provider are serialised by the session; providers that share
  a resource (a file handle) lock it themselves.
- **Metadata** carries what the source has, with the source's names; units
  as `unit:<key>` text; numbers as `double`, integers as `int64`. Filters
  compare numbers as `double`, so identifiers must stay below 2^53 (split
  wider ones, as the EDM4hep adapter does with object IDs).
- **Capabilities** are complete: description, kinds, extensions, options
  with defaults, notes and limitations. `help adapter <name>` and the
  generated {doc}`../reference/adapters` are produced from them.

Tests (in `tests/adapters/<name>/`): a tiny committed fixture in
`tests/data/<name>/` with the committed program or script that generates it;
checks of the series list and counts, one element's geometry and metadata,
a lazy read of entry *n*, bad input producing a report entry, and a golden of
the help text. Tests that need a third-party library carry the labels
`needs-<lib>` (see {doc}`../release/dependencies`).

## Writing a backend

A backend implements `fccvis::backend::Backend` (`Backend.h`). The session
calls it from any thread and expects every call to return quickly; the
backend does its drawing on its own thread. Most backends that write files
derive from `FileBackend`, which implements the protocol once (options
`width`, `height`, `output_dir`, `tolerance`, `async`; revision diffing;
retained per-layer drawings with styles resolved and filters applied;
coalesced rendering on a worker thread; `Flush` and draw statistics). Such a
backend only declares its capabilities and writes its files:

```{literalinclude} ../../examples/extend_backend.cpp
:language: cpp
:start-after: "// [backend]"
:end-before: "// [backend]"
```

```{literalinclude} ../../examples/extend_backend.cpp
:language: cpp
:start-after: "// [use]"
:end-before: "// [use]"
```

What the interface asks for:

| Call | Contract |
|---|---|
| `GetCapabilities()` | element kinds, style properties, splits/tabs support, options with descriptions; the session warns once per layer a backend cannot draw |
| `Start(id, port, options)` | read options (throw `std::invalid_argument` for bad ones), start the target; the `Port` is how the backend talks back |
| `OpenScene` / `CloseScene` | the scenes this instance shows; the session prunes the layout to them |
| `Sync(request)` | the session state changed: compare revisions (`RevisionTracker`) and redraw only what changed |
| `Flush()` | wait until everything requested so far is drawn |
| `Stats()` | optional draw counters; the contract tests use them to check revision diffing |
| `Stop()` | release everything; the instance is not reused |

An interactive backend reports back through the `Port`: `Submit` a change
(for example `ChangeCameraDirection` when the user rotates a view; the
session rejects it for scenes with a fixed camera and asks the backend to
redraw), `Picked` for a selected element (scene, layer, entry, index),
`SceneClosed`, `Stopped` (its window was closed) and `Log`. Port calls may
come from any thread. The ROOT backend ({doc}`../backends/root`) is the
example of a backend with its own GUI thread.

Shared drawing helpers are in `Drawing.h` (`draw::BuildLayer`, `View`,
`Arrange` for layouts, the painter's display list `Paint` used by the raster
and SVG backends, number and XML formatting).

## Registries

Both kinds register at static initialisation with a `Registrar`
(`adapters::Registrar`, `backend::Registrar`) in their `.cc` file. The
library is made of OBJECT modules, so every registrar of an enabled
component is linked into `libFCCVisualization` and into static builds
without `--whole-archive`; a component in your own executable (as in the
examples above) is registered as soon as its translation unit is linked.
`Registry::Global()` lists the names (`Names()`), creates instances
(`Create(name)`) and prints help (`Help(name)`); `RegisterAlias` keeps a
former backend name working (`fv` for `raster`). Names must be unique: a
second registration under the same name is refused.

## Contract tests to pass

Every backend in the library runs the same two suites, once per type:

- `contract::<type>` (`tests/backend/backend_contract_tests.cc`): open draws,
  close removes, detach releases; a filter, style or camera change redraws
  only what changed (with `Stats()`); split and tab layouts are accepted;
  unsupported content is logged, never fatal; two instances coexist.
- `matrix::<type>` (`tests/backend/backend_matrix_tests.cc`): a scripted shell
  session (scenes, layers, filter, style rule, camera change, labelled split,
  closing a pane) in which every command succeeds, nothing is logged as an
  error and each step redraws exactly what it touched.

A new backend type is added to both by appending `<type>:<labels>` to the
`_contract` list in `tests/CMakeLists.txt` under its option (labels
separated by commas, for example `root:needs-root,needs-x11`; a label
`needs-x11` runs it under `FCCVIS_X11_LAUNCHER`), or at configure time with
`-DFCCVIS_CONTRACT_BACKENDS=<type>:<labels>`. Target-specific behaviour
(pictures, picks, files) gets its own tests next to them. The backends of
these examples live in the examples' executables and are not part of the
contract runs.

## Documentation

Every public header is documented for Doxygen (the docs build fails on an
undocumented public symbol); a new adapter or backend gets a page under
`docs/source/adapters/` or `docs/source/backends/` with a compiled or
script example, and appears in the generated reference automatically.
