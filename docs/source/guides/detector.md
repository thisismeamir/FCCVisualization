# Loading DD4hep geometry

The detector adapter turns a DD4hep compact description into one surface
series per sub-detector, named `geometry/<sub-detector>`. Reference:
{doc}`../adapters/detector`.

## Load and inspect

```{literalinclude} ../../examples/guides/detector.fcv
:language: text
:start-after: "# [load]"
:end-before: "# [load]"
```

- `depth=2` keeps two volume levels below each sub-detector: deeper volumes
  are drawn as part of their ancestor at that level. Lower depths give fewer,
  larger surfaces and load faster; raise it to see modules or slices.
- `include=` and `exclude=` take sub-detector name patterns (`*`, `?`).
- `series geometry/*` lists what was registered; `describe` shows the
  metadata of the volumes (`layer`, `material`, `system_id`, ...), which is
  what filters and style rules can use.

DD4hep prints its own log while loading (it goes to the terminal, not into
fccvis' log).

## Choose and style volumes

```{literalinclude} ../../examples/guides/detector.fcv
:language: text
:start-after: "# [layers]"
:end-before: "# [layers]"
```

Filters on geometry work like filters on hits: here only the silicon
volumes of the first two layers are shown. The `detector-outline` preset
draws surfaces as light outlines; a `when` rule then highlights one layer.
Volume metadata comes from DD4hep's volume IDs, so the attribute names are
those of the readout (`layer`, `module`, `side`, ...).

## Draw

```{literalinclude} ../../examples/guides/detector.fcv
:language: text
:start-after: "# [draw]"
:end-before: "# [draw]"
```

## Loading the same file again

```{literalinclude} ../../examples/guides/detector.fcv
:language: text
:start-after: "# [reopen]"
:end-before: "# [reopen]"
```

A geometry that is already loaded is not loaded twice: the report lists the
existing series as `already registered`, and options of the second `load`
(here the default depth) do not apply. Start a new session to load a file
with different options.

Output of the whole script (DD4hep's own log lines included):

```{literalinclude} ../../examples/guides/detector.out
:language: text
```
