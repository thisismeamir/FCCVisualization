/**
 * @file DetectorAdapter.h
 * @brief A1: detector geometry (DD4hep compact XML, or a TGeo ROOT/GDML file)
 *        -> one surface series per sub-detector.
 *
 * Registered as "detector". For a compact file the sub-detectors are DD4hep's
 * top-level detector elements; for a `.root`/`.gdml` geometry they are the
 * daughters of the top volume. Each sub-detector becomes the series
 * `geometry/<sub-detector>` with one entry: one SurfaceElement per volume at
 * the depth limit (or leaf volume above it), in world coordinates (mm).
 * Assemblies have no shape and are walked through. Volumes whose solid has
 * no tessellation (boolean/composite shapes) go to the report's skipped list,
 * counted per sub-detector and solid type.
 *
 * Metadata: `subdetector`, `volume` (volume name), `path` (placement path),
 * `material`, `solid` (ROOT shape class), `system_id` and the other DD4hep
 * volume IDs accumulated along the path (`layer`, `module`, ...) as integers.
 *
 * Shapes stay owned by the geometry; every element keeps it alive.
 * Tessellation (TGeoShape::GetBuffer3D) is serialised by a process-wide lock
 * because TGeo's buffers are shared.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include "Adapter.h"

namespace fccvis::adapters {

/** @brief Detector geometry adapter (A1). */
class DetectorAdapter final : public Adapter {
 public:
  [[nodiscard]] std::string Name() const override { return "detector"; }
  [[nodiscard]] Capabilities GetCapabilities() const override;
  LoadReport Open(data::SessionData& data, const std::string& path, const Options& options) override;
};

}  // namespace fccvis::adapters
