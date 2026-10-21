# Phoenix adapter fixtures

- `events.json`: hand-written, two events covering every mapping of the adapter
  (points as `pos`, plain arrays and `x`/`y`/`z`; tracks with several and one
  position; a jet collection without positions; a collection name used by two
  types; a 64-bit id; a non-event top-level member).
- `ATLAS_calibration.json`: copied unchanged from the Phoenix repository
  (HSF/phoenix, `packages/phoenix-ng/projects/phoenix-app/src/assets/files/event_data/ATLAS_calibration.json`,
  Apache License 2.0), a small real event file written by ATLAS software.

Larger real files (for example Phoenix's `atlas.json`, 30 MB) are read by the
`needs-data` test when `FCCVIS_TEST_PHOENIX_FILE` names one.
