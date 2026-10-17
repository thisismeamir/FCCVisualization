# Phoenix JSON adapter

Registry name: `phoenix`. Built when `FCCVIS_ENABLE_PHOENIX_JSON` is on
(default; no dependency). It reads the event-data JSON of the
[Phoenix](https://github.com/HSF/phoenix) event display — the format the
{doc}`Phoenix backend <../backends/phoenix>` writes and that experiments
export for Phoenix — and registers one series per collection; an entry is
one event.

```{literalinclude} ../../examples/guides/phoenix_json.fcv
:language: text
:start-after: "# [phoenix-json]"
:end-before: "# [phoenix-json]"
```

```{literalinclude} ../../examples/guides/phoenix_json.out
:language: text
:end-before: "--- stderr"
```

## Mapping

A file is either an object of events (each event an object with collection
types such as `Hits`, `Tracks`, `Vertices`, each holding named lists of
objects) or a single event. Members of the top level that are not events are
reported as skipped.

| Objects of a collection | Element |
|---|---|
| `pos: [x, y, z]`, a plain `[x, y, z]`, or `x`, `y`, `z` members (hits, vertices) | point |
| `pos: [[x, y, z], ...]` (tracks) | polyline through the given positions |
| only `eta`, `phi`, `energy` (jets, calorimeter clusters and cells), missing energy | skipped: no positions |

The series is named like the collection; when two collection types use the
same name (`Hits/shared`, `Tracks/shared`) both get the type as prefix. A
collection missing from an event gives an empty entry. Positions are taken
as mm (Phoenix's unit).

Tracks are not extrapolated: Phoenix draws a track that has only one
position, or only parameters, as a helix computed from `dparams` and its own
magnetic-field assumption; this adapter keeps the given positions (one
vertex: nothing to draw) and the parameters as metadata.

## Metadata

Every other member of an object, with its JSON type: integers exactly (64
bits, so detector identifiers survive), numbers, text and booleans.
`dparams` becomes `d0`, `z0`, `phi`, `theta`, `qOverP`; other short numeric
arrays become `<name>_0`, `<name>_1`, ... Objects and long arrays are left
out.

## Reading large files

The file is read into memory and scanned once for its events and
collections; an event is parsed when one of its entries is first needed (a
30 MB ATLAS file with 190 000 calorimeter cells opens in about 2 s here).

The fixtures are a hand-written file covering every mapping and
`ATLAS_calibration.json` from the Phoenix repository; with
`FCCVIS_TEST_PHOENIX_FILE` set, a `needs-data` test reads a large real file
(checked with `atlas.json` and `LHCbEventData.json` from the Phoenix
application's example files).
