/**
 * @file Edm4hepAdapter.h
 * @brief A2: EDM4hep event files (podio frames) -> one series per collection.
 *
 * Registered as "edm4hep". Opens any file podio::makeReader accepts (podio
 * ROOT files, TTree or RNTuple based). Every collection of the first event
 * frame whose type is mapped becomes a series named like the collection; an
 * entry is one event frame (category "events").
 *
 * Mapping by collection type:
 * - `SimTrackerHit`, `TrackerHit3D`, `TrackerHitPlane`, `CalorimeterHit`,
 *   `SimCalorimeterHit`, `Cluster`: points at the stored position;
 * - `Track`: polyline through the positions of its track states, ordered by
 *   location (IP, first hit, last hit, calorimeter, other), the position of a
 *   state being `referencePoint + (-D0 sin(phi), D0 cos(phi), Z0)`; option
 *   `track=hits` uses the positions of the track's hits instead, ordered by
 *   distance from the origin;
 * - `MCParticle`: straight segment from vertex to endpoint (when the endpoint
 *   equals the vertex, `mc_length` mm along the momentum; `endpoint_set` is
 *   false);
 * - `ReconstructedParticle`: segment from the reference point, `reco_length`
 *   mm along the momentum.
 *
 * Metadata uses the EDM4hep member names (`cellID`, `EDep`, `energy`,
 * `time`, `PDG`, `charge`, `generatorStatus`, ...), derived values (`p`,
 * `pt`) and relation counts (`hits`, `tracks`, `clusters`, `parents`,
 * `daughters`, `contributions`). Every element has `index` and
 * `index_collection` (its podio object ID: index in the collection and
 * collection ID); a single-valued relation is stored the same way under the
 * relation's name (`particle`, `particle_collection` of a SimTrackerHit).
 * Both are kept as separate integers so that they stay exact when filters
 * compare them as numbers. No joins are made.
 * Units: mm, GeV, ns (`unit:<key>` metadata).
 *
 * Collections of other types (links, event headers, raw hits without a
 * position, user data) are reported as skipped with the reason.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include "Adapter.h"

namespace fccvis::adapters {

/** @brief EDM4hep event-data adapter (A2). */
class Edm4hepAdapter final : public Adapter {
 public:
  [[nodiscard]] std::string Name() const override { return "edm4hep"; }
  [[nodiscard]] Capabilities GetCapabilities() const override;
  LoadReport Open(data::SessionData& data, const std::string& path, const Options& options) override;
};

}  // namespace fccvis::adapters
