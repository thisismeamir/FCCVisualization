/**
 * @file PhoenixAdapter.h
 * @brief A4: Phoenix event-data JSON -> one series per collection.
 *
 * Registered as "phoenix". Reads the event-data format of the Phoenix event
 * display (HSF/phoenix, `guides/developers/event_data_format.md`): a JSON
 * object of events (each with collection types such as `Hits`, `Tracks`,
 * `Vertices`, each holding named collections of objects), or a single event.
 * Every collection whose objects carry positions becomes a series named like
 * the collection (prefixed with its type, `Tracks/<name>`, when two types use
 * the same name); an entry is one event, in file order.
 *
 * Mapping by what the objects contain:
 * - `pos: [x, y, z]`, `[x, y, z]` (plain array) or `x`, `y`, `z` members:
 *   a point;
 * - `pos: [[x, y, z], ...]`: a polyline through the given positions (tracks
 *   are not extrapolated from their parameters; a track with one position
 *   gives a one-vertex line).
 * Collections without positions (jets, calorimeter clusters and cells given
 * by eta/phi, missing energy) are reported as skipped.
 *
 * Metadata: the other members of each object with their JSON types (integers
 * exact up to 64 bits, numbers, text, booleans); `dparams` becomes `d0`,
 * `z0`, `phi`, `theta`, `qOverP` (Phoenix's track parameters); other short
 * numeric arrays become `<name>_0`, `<name>_1`, ... (array elements are
 * always floating-point numbers; scalar integers stay integers). Positions are taken as mm
 * (Phoenix's unit). The file is scanned once; an event is parsed when one of
 * its entries is first needed.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include "Adapter.h"

namespace fccvis::adapters {

/** @brief Phoenix event-data JSON adapter (A4). */
class PhoenixAdapter final : public Adapter {
 public:
  [[nodiscard]] std::string Name() const override { return "phoenix"; }
  [[nodiscard]] Capabilities GetCapabilities() const override;
  LoadReport Open(data::SessionData& data, const std::string& path, const Options& options) override;
};

}  // namespace fccvis::adapters
