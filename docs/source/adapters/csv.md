# CSV adapter

Registry name: `csv`. Built when `FCCVIS_ENABLE_CSV` is on (default; no
dependencies). Reads delimiter-separated text with a header row into one series
of points (one per row) or polylines (rows grouped by a column).

```{literalinclude} ../../examples/adapter_csv.cpp
:language: cpp
:start-after: "// [open]"
:end-before: "// [open]"
```

`Open` scans the file once: it indexes the rows of every entry, infers the
column types and reports malformed rows. Loading entry *n* later reads only
the rows of entry *n*. Opening the same file again reports the existing series
(`existing = true`) and registers nothing new.

## Mapping

| Input | Result |
|---|---|
| each row (`kind=point`) | one `PointElement` at (`x`, `y`, `z`) |
| rows with the same `group` value (`kind=line`) | one `LineElement`, vertices ordered by `order` (file order if not given); metadata from the first row |
| `entry` column | rows with the same value form one entry: integer values ascending, other values by first appearance; without it the file is one entry |
| other columns (or the `columns` list) | metadata, typed per column over the whole file: integer, else number, else `true`/`false`, else text; empty cells give no key |
| header `energy [GeV]` | column `energy`, plus metadata `unit:energy = GeV` |
| coordinates in `um`, `mm`, `cm`, `m` | converted to millimetres; the series description says so. Other units skip the file |

Series name: option `name` (default: the file stem). Series source:
`csv:<absolute path>`.

## Options

The table is the adapter's generated help (`fccvis help adapter csv`), which
the tests compare with `tests/data/csv/help.txt`:

```{literalinclude} ../../../tests/data/csv/help.txt
:language: text
```

## Report

`LoadReport::ToString()` lists the registered series (kind, entries, metadata
keys) and the skipped items with reasons, sorted; malformed rows appear as
`line N` in file order. With `strict=true`, any malformed row rejects the file
and nothing is registered. Invalid options throw `std::invalid_argument`; bad
content never does.
