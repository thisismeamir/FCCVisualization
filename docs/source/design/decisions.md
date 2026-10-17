# Architecture decisions

fccvis is built on thirteen decisions recorded in `ARCHITECTURE.md` before
the v1.0 work. This page gives the reasoning behind each, where it lives in
the code, and the interpretations made while implementing it (also listed in
`STATUS.md` for review).

## Three layers, one direction

*Adapters (source → series) → Session (data, scenes, layout, changes) →
Backends (draw).* Only the session talks to backends; adapters never see the
session or scenes.

The predecessor, PandoraMonitoring, mixed reading, selecting and drawing in
one API, so every new view meant new code paths through all three. With the
layers separated, a new input format is one adapter that every backend can
draw, and a new output is one backend that every source can feed. The module
graph (`cmake/FCCVisModules.cmake`) makes the direction a build error rather
than a convention: modules are OBJECT libraries with an allowed-dependency
table, third-party libraries may only be linked by adapters and backends,
and an include checker verifies that each module includes only what it
declares. The core therefore builds with the C++ standard library alone.

## 1. Data model

`SessionData` (`Data.h`) is a typed, read-only store keyed by
(element type, series name, source). A series is a list of entries (events);
providers are lazy and entries are converted once into a small per-series
cache, returned as shared immutable vectors.

*Why.* Changing a filter or a style must never re-read a file, and a
display rarely needs more than the current event. Shared immutable entries
let backends keep drawing an entry while the session moves on, without
copies or locks on the elements. Keeping the cursor (which entry is shown)
in the layer rather than in the store lets two layers show different events
of one series. Geometry needs no special case: it is a surface series with
one entry.

*Interpretation.* The source of a series is `"<adapter>:<absolute path>"`
rather than the adapter's name, so equal series names from two files do not
collide; a reference with an empty source still finds a unique name.

## 2. Elements

An element is a geometry (`Point`, `Line`, `Surface`) plus `Metadata`
(named bool, int64, double or text values). Domain meaning (hit, track,
volume) lives in series names and metadata, not in types.

*Why.* A small closed set of geometry kinds is what every backend can draw;
open-ended metadata is what every filter and style can use. Adding "a
calorimeter hit type" would multiply code across adapters, filters, styles
and backends without changing what is drawn. A filter on a missing attribute
never passes, so a selection never silently includes elements that lack the
information.

## 3. Adapters expose the source

Adapters convert what the file contains and derive nothing. Where a curve
must become a polyline, the adapter samples it and says so in its
capabilities.

*Why.* A display that silently recomputes physics quantities is hard to
trust. The EDM4hep adapter, for example, joins the stored track states and
does not extrapolate a helix; MC particles without an endpoint get a segment
along their momentum and the attribute `endpoint_set=false`.

## 4. Scenes hold no data

A scene is a camera, camera options, an environment style and a set of
layers; a layer is a series reference, an entry, a filter and a style
sheet. Scene names are unique; one scene can appear in several panes.

*Why.* Scenes are descriptions, cheap to copy, compare and save: the shell's
configuration files are just the commands that build them. Data stays in one
place (the store), so two scenes showing the same series share its cache.

## 5. Merging

`Mergeable<T>` (CRTP over a `Members()` tuple) gives every style record
`TakeRef` (the other value wins where it is set) and `CompleteBy` (fill only
unset fields). The style cascade and backend defaults are written once with
these two operations.

## 6. Styling by rules

A style sheet is an ordered list of rules `{when, style}`; later rules win
property by property, and the backend completes what no rule set with its
defaults. All style fields are optional.

*Why.* Rules with conditions express the common display needs (colour by
energy, highlight a sub-detector, ghost the rest) without per-element style
storage, and optional fields let a rule change one property and leave the
others to earlier rules. Backends declare which properties they honour, so
one sheet serves a PNG writer and an interactive viewer.

## 7. Layout as a value tree

Panes, splits (with labels and weights) and tabs, validated and pruned:
`Prune` drops panes of scenes a backend has not opened, empty containers,
and collapses single-child containers.

*Why.* A value tree is easy to save, compare and send to each backend
pruned to what that backend shows. Static backends (raster, SVG) draw the
first tab of a tab group; the ROOT backend shows real tabs.

## 8. Backends materialize

Backends are thin bridges to existing libraries and build their own
representation from the scene description; there is no shared drawable
intermediate representation. Each instance has its own set of open scenes,
declares its capabilities, returns from session calls quickly and reports
events through its `Port`.

*Why.* Each target (ROOT Eve, SVG, a histogram, Phoenix JSON) has its own
natural representation; a common IR would be the least common denominator
or would grow into a toolkit of its own. Shared code for the software
backends exists as helpers on the backend side (`Drawing.h`: resolved
per-layer drawings, projection, the painter's display list used by both the
raster and SVG writers), not as an exchange format.

*Interpretation.* Images are produced on every synchronisation that changed
something (there is no separate render call); `Session::Flush` waits for
them.

## 9. Sync by revisions, no observers

The session keeps revision counters per scene part and per layer; a backend
remembers what it last drew and redraws what changed (`RevisionTracker`).
Changes that a backend itself caused (a camera rotated in its window) are
recorded without redrawing it.

*Why.* Observers would call into backends from inside a change, on the
caller's thread and with locks held. Revisions let each backend pull the
state when it is ready, coalesce bursts of changes, and catch up after being
busy. The contract tests check through draw statistics that a filter change
redraws one layer and a camera change only the camera.

## 10. One mutation path

Every change goes through `Session::Resolve`: validate, apply, log, bump
revisions, synchronise backends. Changes are values with a name, a
description and a footprint; a batch stops at its first failure.

*Why.* One path means one place for validation, logging, undo (planned) and
consistency, whatever the front end: C++ builders, the shell, saved
configurations and backend events all produce the same change objects.
`api::Script` holds shared immutable changes, so scripts are copyable values
that can run on several sessions.

## 11. ROOT on one thread

All ROOT graphics run on one thread owned by a process-wide runtime shared by
all ROOT backend instances; no `TEve*` pointer leaves the backend. The
viewer (classic TEve or web-based REve) was decided by experiments.

*Why.* ROOT's GUI and GL code is not thread-safe and keeps global state
(`gEve`, `gApplication`). The experiments ({doc}`root-experiments`) found
classic TEve fully workable (event loop pumped with
`gSystem->ProcessEvents()`, picks and camera moves observable, closing the
window detectable, offscreen pictures), while REve's picking, camera and
close behaviour could not be verified without a browser. So the TEve backend
came first; REve remains the planned web viewer.

*Interpretation.* With ROOT 6.38, closing the Eve window deletes `gEve` but
does not end the process; the backend reports `Port::Stopped` and a new ROOT
backend cannot be started in that process.

## 12. The session does not move

`Session` is neither copyable nor movable, because the ports it gives to
backends hold a pointer to it.

## 13. Registries

Adapters and backends register by name at static initialisation; each is
optional through a CMake option, and a disabled one is not compiled. OBJECT
libraries keep every registrar in the final library without whole-archive
linking; on Linux the shared library is also linked with `--no-as-needed`
for consumers, so registrars run even when no symbol of the library is
referenced directly.

## Other choices made during v1.0

- **No PandoraSDK dependency.** No code used it; the compatibility facade for
  PandoraMonitoring was not part of v1.0.
- **Millimetres** are the length unit everywhere; the TGeo/DD4hep boundary
  converts from centimetres.
- **The configuration format is the command language.** A saved session is
  the list of shell commands that rebuild it (`fccvis-config` JSON, version
  1), replayed through the interpreter and thus `Resolve`. Filters written
  as C++ lambdas have no text form and are not saved.
- **Fixed cameras** are locked against interaction (changes from backends),
  not against the user or the API.
- **Standard-library filters** return a `Selector` convertible to
  `Filter<X>` for all three element kinds, so one expression serves any
  layer; positional filters (`first(n)`, `index(i)`) receive the element's
  index.
- **Own PNG codec** (deflate with fixed Huffman codes for writing, a full
  inflate for reading) instead of a zlib dependency, so the raster backend
  needs nothing outside the standard library.
