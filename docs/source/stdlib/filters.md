# Filters

Header: `fccvis/stdlib/Filters.h`. Standard filters return a `Selector`: one
`filter::Filter<X>` per built-in element kind (point, line, surface). A
`Selector` converts implicitly to `Filter<X>`, composes with `&`, `|`, `!`,
and mixes with hand-written `Filter<X>` predicates.

## Attributes and energy

Attribute names are arguments; adapters document the names they set.
Every comparison fails when the attribute is missing or has another type
(text compared numerically), including `!=`. Negation is plain logic:
`!EnergyAbove(1)` passes elements with no energy; add `Exists("energy")` if
that matters.

| Filter | Passes when |
|---|---|
| `Attr<T>(name) == v`, `!=`, `<`, `<=`, `>`, `>=` | comparison holds; `T` is `double`, `std::int64_t`, `bool` or `std::string` |
| `Between(name, lo, hi)` | numeric value in `[lo, hi]` |
| `OneOf(name, {...})` | value equals one of the numbers or strings |
| `Exists(name)` | attribute present |
| `Matches(name, regex)` | text fully matches the ECMAScript regex |
| `EnergyAbove(x)`, `EnergyBelow(x)`, `EnergyBetween(lo, hi)` | same on attribute `energy` (optional last argument renames it) |

```{literalinclude} ../../examples/stdlib_filters.cpp
:language: cpp
:start-after: "// [attributes]"
:end-before: "// [attributes]"
:dedent: 2
```

## Selection helpers

`Index(i)` and `FirstN(n)` are positional: they use the element's index in the
drawn entry, which `Filter::Apply`, `Scene::Select` and `Scene::Prepare`
supply. `All()` / `None()` (and `All<X>()` / `None<X>()`) select everything or
nothing.

```{literalinclude} ../../examples/stdlib_filters.cpp
:language: cpp
:start-after: "// [selection]"
:end-before: "// [selection]"
:dedent: 2
```

## Shape context

Header: `fccvis/stdlib/ShapeFilters.h`. `Inside(shape)`, `Outside(shape)` and
`Within(distance, shape)` take a placed `geometry::Shape` or a shape in world
coordinates. Points are tested at their position; lines by `LineTest`
(`AnyVertex` (default), `AllVertices`, `Centroid`); surfaces at the centre of
their shape's bounding box. `Within` uses the shape's criterion as a distance
estimate (millimetres).

```{literalinclude} ../../examples/stdlib_filters.cpp
:language: cpp
:start-after: "// [shapes]"
:end-before: "// [shapes]"
:dedent: 2
```

## Detector sub-parts

Header: `fccvis/stdlib/DetectorFilters.h`. Sugar over attribute filters, with
no built-in detector knowledge; the default keys (`stdlib::keys`) are the ones
adapters set:

| Filter | Default key | Type |
|---|---|---|
| `Subdetector(name)` | `subdetector` | text, exact |
| `Layer(n)`, `LayerRange(a, b)` | `layer` | integer (range inclusive) |
| `Volume(regex)` | `volume` | text, full regex match |
| `Material(name)` | `material` | text, exact |
| (key only) | `system_id` | integer |

Each filter accepts another attribute name as its last argument.

```{literalinclude} ../../examples/stdlib_filters.cpp
:language: cpp
:start-after: "// [detector]"
:end-before: "// [detector]"
:dedent: 2
```

## Not provided

Kinematic filters such as transverse momentum are not provided: adapters
expose stored values only, and no standard format stores pT directly. When an
adapter stores such an attribute, use `Attr<double>(name)`.
