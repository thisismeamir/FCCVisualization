# Detector adapter (DD4hep)

Registry name: `detector`. Built when `FCCVIS_ENABLE_DD4HEP` is on (default
when DD4hep's DDCore and ROOT's Geom are found; tested with DD4hep v01-38 and
ROOT 6.38.06). It reads a DD4hep compact description (`.xml`) or a TGeo
geometry (`.root`, `.gdml`) and registers one surface series per
sub-detector.

```{literalinclude} ../../examples/detector_session.fcv
:language: text
:start-after: "# [detector]"
:end-before: "# [detector]"
```

```{image} ../_static/detector.png
:alt: The toy detector of the tests seen from the side
:width: 360px
```

DD4hep loads detector drivers as plugins found through `LD_LIBRARY_PATH`:
put DD4hep's `lib` directory (and the libraries of your detector drivers)
there before starting fccvis. DD4hep prints its own log while loading.

## Mapping

| Source | Result |
|---|---|
| compact file | sub-detectors = DD4hep's top-level detector elements; world transforms from DD4hep (`nominal().worldTransformation()`) |
| `.root` / `.gdml` | sub-detectors = daughters of the top volume |
| sub-detector | series `geometry/<name>`, one entry |
| volume at the depth limit, or leaf above it | one `SurfaceElement`: the TGeo shape (wrapped, mm) placed in world coordinates |
| assembly | walked through (no shape) |
| boolean (composite) solid | skipped, counted per sub-detector and solid type in the report |

Metadata of every element: `subdetector`, `volume`, `path`, `material`,
`solid` (ROOT shape class), `system_id` and the other DD4hep volume IDs along
the placement path (`layer`, `module`, `slice`, ...), so the
[detector filters](../stdlib/filters.md) (`Subdetector`, `Layer`, `Material`)
and shell expressions such as `layer >= 2 and material == "Silicon"` work
directly.

Options (`depth`, `include`, `exclude`, `max_volumes`) and their defaults are
in the [adapter reference](../reference/adapters.md) (when the documentation was built with the detector adapter; `help adapter detector` in the shell always lists them). Shapes stay owned
by the geometry, which every element keeps alive; tessellation goes through
`TGeoShape::GetBuffer3D` behind a process-wide lock.

## Large detectors

`max_volumes` (default 200000) bounds the memory used by one `load`. The
budget is shared by the sub-detectors in their order in the geometry; a
sub-detector cut by it is still registered with the volumes read so far and
reported as `max_volumes reached: incomplete, N volume(s) kept`, and the
sub-detectors after it are reported as `max_volumes reached`. Measured with
the CLD detector (`CLD_o2_v07` from k4geo): 324328 volumes in 39 sub-detector
series, loaded in about 4 s; drawing all of them into one PNG took about 20 s
and 3 GB of memory. Load it with `max_volumes=400000`, or keep the default and
select sub-detectors with `include`/`exclude` or a lower `depth`.

## Testing with a real detector

The `needs-data` tests load the detector named by `FCCVIS_TEST_COMPACT`
(written for CLD) and skip themselves when it is unset. Its detector drivers
must be on `LD_LIBRARY_PATH`: with Key4hep they are; otherwise
`tools/k4geo_cld` builds the drivers CLD needs from a k4geo checkout, against
DD4hep alone (see the comment at its top).
