# EDM4hep adapter (podio)

Registry name: `edm4hep`. Built when `FCCVIS_ENABLE_EDM4HEP` is on (default
when EDM4hep and podio with its ROOT I/O are found; tested with podio v01-08,
EDM4hep v01-01-01 and ROOT 6.38.06). It reads EDM4hep event files written by
podio (the frame-based ROOT format, TTree or RNTuple) and registers one
series per collection; an entry is one event.

```{literalinclude} ../../examples/edm4hep_session.fcv
:language: text
:start-after: "# [edm4hep]"
:end-before: "# [edm4hep]"
```

```{image} ../_static/edm4hep.png
:alt: Calorimeter hits, vertex hits, a track and MC particles of event 2 of the test fixture
:width: 360px
```

podio finds the EDM4hep data-model dictionaries through `LD_LIBRARY_PATH`:
put the `lib` directories of EDM4hep, podio and ROOT there before starting
fccvis (a Key4hep environment does this).

## Mapping

| Collection type | Element | Geometry |
|---|---|---|
| `SimTrackerHit`, `TrackerHit3D`, `TrackerHitPlane` | point | stored position |
| `CalorimeterHit`, `SimCalorimeterHit` | point | stored position |
| `Cluster` | point | stored position |
| `Track` | line | positions of the track states, ordered at IP, first hit, last hit, calorimeter, other; the position of a state is `referencePoint + (-D0 sin(phi), D0 cos(phi), Z0)`. With `track=hits`: the positions of the track's hits, ordered by distance from the origin |
| `MCParticle` | line | vertex to endpoint; if the endpoint equals the vertex (not set), `mc_length` mm along the momentum and `endpoint_set` is false |
| `ReconstructedParticle` | line | from the reference point, `reco_length` mm along the momentum |
| other types | — | skipped, with the reason in the load report (`RawCalorimeterHit`: no position; `EventHeader`; link collections; user data) |

Collections are discovered in the first event. A collection missing from a
later event gives an empty entry.

Tracks are not extrapolated: the polyline joins the stored states (no helix
is drawn between them). Use `track=hits` for a line through the measured
points.

## Metadata

Attribute names are the EDM4hep member names, so filters read like the data
model: `where event/calo energy > 1`, `where event/mc PDG == 11 and
generatorStatus == 1`.

| Collection type | Attributes |
|---|---|
| `SimTrackerHit` | `cellID`, `EDep`, `time`, `pathLength`, `quality`, `p` (momentum magnitude), `particle`, `particle_collection` |
| `TrackerHit3D`, `TrackerHitPlane` | `cellID`, `EDep`, `time`, `type`, `quality` |
| `CalorimeterHit` | `cellID`, `energy`, `time`, `type` |
| `SimCalorimeterHit` | `cellID`, `energy`, `contributions` (count) |
| `Cluster` | `energy`, `type`, `hits`, `clusters` (counts) |
| `Track` | `type`, `chi2`, `ndf`, `hits`, `states` (counts) |
| `MCParticle` | `PDG`, `charge`, `energy`, `mass`, `p`, `pt`, `generatorStatus`, `simulatorStatus`, `endpoint_set`, `parents`, `daughters` (counts) |
| `ReconstructedParticle` | `PDG`, `charge`, `energy`, `mass`, `p`, `pt`, `goodnessOfPID`, `tracks`, `clusters`, `particles` (counts) |

Every element also has `index` and `index_collection`: its podio object ID
(index in the collection, collection ID). A single relation is stored the
same way under the relation's name (`particle` and `particle_collection` of
a `SimTrackerHit` point to its `MCParticle`); relations to several objects
are stored as counts. No joins are made in v1.0: to find the MC particle of
a hit, filter the MC particle layer on `index`. The two parts are kept as
separate integers so that comparisons stay exact (filters compare numbers
as double precision). `cellID` is stored as a signed 64-bit integer.

Units: positions in mm (EDM4hep's unit), energies and momenta in GeV, times
in ns, given as `unit:<attribute>` metadata.

## Options

`collections` and `exclude` take comma-separated name patterns (`*`, `?`);
`track`, `mc_length`, `reco_length` and `cache` are described in
`help adapter edm4hep` (and in the [adapter reference](../reference/adapters.md)
when these pages were built with the adapter).

## Testing

The fixture `tests/data/edm4hep/events.edm4hep.root` (3 events, every mapped
type and a few skipped ones) is written by the committed program
`tests/data/edm4hep/make_fixture.cc` (target `fccvis_edm4hep_make_fixture`).
Tests are labelled `needs-root`, `needs-podio`, `needs-edm4hep`.
