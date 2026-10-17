# Styles

Header: `fccvis/stdlib/Styles.h`. Everything here produces ordinary style data:
lists of `style::StyleRule<X>`, `style::StyleSheet<X>` values, and
`style::EnvironmentStyle`. The cascade is unchanged: start from the sheet's
`base`, apply matching rules in order (later rules win), then the backend
completes the result with its defaults (`Resolve(x).CompleteBy(defaults)`).
Backends only ever see the resolved, possibly partial, style.

## Palettes

| Palette | Description |
|---|---|
| `Monochrome(accent)` | light tint of the accent to the accent (default accent `#7017FF`) |
| `Categorical(n)` | n distinguishable colours (14-colour set, cycling) |
| `Sequential(lo, hi)` | two-stop ramp |
| `Diverging(lo, mid, hi)` | three-stop ramp (default blue, light gray, red) |

`Palette::At(t)` interpolates (t clamped to [0, 1]); `Palette::Index(i)` cycles
through the stops.

## Attribute-driven rules

`ColorByAttribute<X>(attr, palette, {lo, hi}, bins)` and
`SizeByAttribute<X>(attr, minSize, maxSize, {lo, hi}, bins)` return one rule per
bin. Bins are half-open; values below/above the range fall into the first/last
bin, so each element with the attribute matches exactly one rule and elements
without it match none. "Size" means point size, line width or surface edge
width. `Highlight<X>(filter)` gives selected elements the accent colour.

```{literalinclude} ../../examples/stdlib_styles.cpp
:language: cpp
:start-after: "// [rules]"
:end-before: "// [rules]"
:dedent: 2
```

## Presets

| Preset | Result |
|---|---|
| `DetectorOutline()` | surfaces: light fill at 15 % opacity, thin dark edges |
| `HitsByEnergy({lo, hi})` | points coloured (monochrome) and sized by `energy` |
| `TracksByCharge()` | lines: negative blue, positive accent, neutral gray dashed (`charge`) |
| `Ghosted<X>()` | everything gray at 15 % opacity; apply to each layer of a scene to ghost it |
| `Publication()` | environment: white background, axes, no grid |
| `DefaultEnvironment()` | environment of the figure theme: white background, light grid |

```{literalinclude} ../../examples/stdlib_styles.cpp
:language: cpp
:start-after: "// [presets]"
:end-before: "// [presets]"
:dedent: 2
```
