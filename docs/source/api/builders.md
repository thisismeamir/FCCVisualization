# Builders and scripts

Header: `fccvis/api/Builders.h` (module `api`). Builders construct changes from
the existing change catalogue; they add no other way to modify a session.
A `Script` is an ordered list of changes that runs as **one batch** through
`Session::Resolve`: changes apply in order, the batch stops at the first
failing change (`Result::applied` says how many were applied), and backends
are notified once at the end.

```{literalinclude} ../../examples/api_builders.cpp
:language: cpp
:start-after: "// [builders]"
:end-before: "// [builders]"
```

## Scene builder

`api::Scene(name)` returns a `SceneBuilder`:

| Method | Change(s) |
|---|---|
| `Create()` | `CreateScene`, `SetCamera` (a camera of its own) |
| `Remove()` | `RemoveScene` |
| `Camera(ptr)` | `SetCamera` (share a camera between scenes) |
| `Direction({x,y,z})`, `Position({x,y,z})` | `ChangeCameraDirection`, `ChangeCameraPosition` |
| `Zoom(m)`, `ResetZoom()` | `ChangeCameraMagnification`, `ResetCameraMagnification` |
| `CameraMode(mode)` | `SetCameraOptions` |
| `Environment(env)` | `SetEnvironment` |
| `RemoveLayer(name)` | `RemoveLayer` |
| `Layer<G>(layer, series, entry)` | `AddLayer<Element<G>>`; returns a `LayerBuilder` |

## Layer builder

`Layer<G>` takes the geometry kind (`data::Point`, `data::Line`,
`data::Surface`). On the returned `LayerBuilder`:

| Method | Change(s) |
|---|---|
| `Entry(n)` | `SetEntry` |
| `Where(filter)` / `All()` | `SetLayerFilter` (stdlib `Selector`s convert implicitly) |
| `Style(style)` | `AddStyleRule` matching every element |
| `Rule(when, style)` | `AddStyleRule` |
| `Sheet(sheet)` / `Rules(rules)` | the sheet's base and rules as `AddStyleRule`s |
| `Layer<G2>(...)` | continues the same scene with another layer |

Both builders convert to `Script`.

## Layout

`api::Pane(scene)`, `api::Split({children}, orientation)` and
`api::Tabs({children})` build layout nodes; a child may carry a label and a
split weight: `{api::Pane("xy"), "Transverse", 2}`. `api::Layout(node)` is a
script with one `SetLayout` change.

## Scripts

`Script` is a value: copies share the immutable changes. `Add(change)`,
`Append(script)`, `Describe()` (one line per change), `Run(session)`.
