# Known limitations

What v1.0 does not do, or does with restrictions. Items marked
**unverified** work against the documented interface but could not be
checked against the real counterpart in the development environment.

## Session, API and shell

- Undo covers changes to scenes, layers, cameras and the layout (100 steps);
  loading data, backends and camera moves made in interactive windows are
  not undoable.
- The Python binding covers sessions, loading, scene builders, scripts,
  shell commands, backends and undo; it does not expose element data or
  backend events. No Kotlin DSL.
- Saved configurations contain the commands that rebuild a session. Filters
  and styles written as C++ lambdas have no text form and are not saved;
  backends are not saved.
- A batch of changes stops at its first failing change; the changes before
  it stay applied (there is no rollback).
- The interactive shell and the Phoenix HTTP host use POSIX interfaces;
  Windows is not supported. The code is tested on Linux with GCC 13.

## Adapters

- **CSV**: quoted fields may not span several lines; the entry, order and
  coordinate columns are not copied into metadata.
- **Detector (DD4hep/TGeo)**: boolean (composite) solids are not tessellated
  and are reported as skipped; TGeo files share ROOT's global geometry
  manager, so one TGeo geometry is loaded per process at a time; loading an
  already loaded file returns the existing series and ignores new options.
  The default `max_volumes` (200000) is below the full CLD detector (324328
  volumes); the sub-detectors beyond it are reported, not loaded (see the
  adapter page).
- **EDM4hep**: collections are discovered in the first event; relations are
  not joined (single relations are stored as object indices, multiple ones
  as counts); tracks join their stored states and are not extrapolated;
  `cellID` is stored as a signed 64-bit integer.
- **Phoenix JSON**: collections given only by eta/phi or track parameters are
  skipped (Phoenix computes their shapes); the file is held in memory.
- No adapter for HepMC3, LCIO or generic ROOT trees.

## Backends

- **Raster and SVG** are static: a tab group shows its first tab; no
  perspective projection (the SVG backend declares this).
- **Histogram**: distributions are configured by backend options only (no
  `HistogramSpec` change); no ROOT `TH1` output.
- **ROOT (TEve)**: needs an X display; split weights are not honoured (Eve
  packs share space equally); closing the Eve window ends TEve for the
  process, so a new ROOT backend needs a restart. Interaction is tested by
  driving the TEve calls a mouse action ends in, under a virtual X server;
  real mouse input on a physical display was not exercised by the tests.
- **ROOT (REve)**: surfaces are drawn as outlines; the browser arranges the
  viewers (no split weights or tabs); camera moves in the browser are not
  reported (REve keeps them client-side); tested with headless Chromium.
- **Phoenix**: checked with the Phoenix event-display library (4.2.3) in
  headless Chromium; the Phoenix web application with its menus was not run.
  Marker shapes and point sizes are not transferred.
- **CED**: one scene at a time; camera and environment are not transferred;
  the protocol uses the host byte order (as CED does).

## Build

- Optional components are tested with the versions in
  {doc}`../release/dependencies` (ROOT 6.38.06, DD4hep v01-38, podio v01-08,
  EDM4hep v01-01-01); other versions are untested.
