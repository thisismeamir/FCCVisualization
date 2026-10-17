# CSV recipes

The CSV adapter reads delimiter-separated text with a header row. Reference:
{doc}`../adapters/csv`. The recipe files are in `docs/examples/data`.

## Points with units and events

```{literalinclude} ../../examples/guides/csv.fcv
:language: text
:start-after: "# [points]"
:end-before: "# [points]"
```

A header cell `energy [GeV]` names the column `energy` and adds the
metadata `unit:energy`; coordinates given in um, cm or m are converted to mm.
`entry=event` groups the rows into entries by the value of that column.

## Other column names and separators

```{literalinclude} ../../examples/data/recipes/cells.tsv
:language: text
```

```{literalinclude} ../../examples/guides/csv.fcv
:language: text
:start-after: "# [columns]"
:end-before: "# [columns]"
```

`delimiter=` accepts one character, `tab` or `space`; `x=`, `y=`, `z=` name
the coordinate columns; `name=` sets the series name (default: the file
name without extension). Lines starting with `#` are comments (`comment=`).

## Lines from grouped rows

```{literalinclude} ../../examples/data/recipes/steps.csv
:language: text
```

```{literalinclude} ../../examples/guides/csv.fcv
:language: text
:start-after: "# [lines]"
:end-before: "# [lines]"
```

`kind=line` makes one polyline per value of the `group` column, with
vertices ordered by the `order` column (rows may come in any order). The
metadata of a line comes from its first row. Malformed rows are reported
with their line number and skipped.

## Strict files

```{literalinclude} ../../examples/guides/csv.fcv
:language: text
:start-after: "# [strict]"
:end-before: "# [strict]"
```

With `strict=true` one malformed row rejects the whole file; the command
fails, and a script stops there (`fccvis-shell --keep-going` continues).

Output:

```{literalinclude} ../../examples/guides/csv.out
:language: text
```
