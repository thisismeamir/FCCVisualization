#pragma once
/**
 * @file Adapter.h
 * @brief Contract of an adapter: a source (file, URI) turned into series.
 *
 * An adapter registers lazy series in a data::SessionData and returns a
 * LoadReport saying what it registered and what it skipped (with reasons). It
 * never aborts on one bad collection, row or volume. Adapters depend only on
 * the data module and their own third-party library; they never see the
 * session, scenes or backends.
 *
 * Conventions (SPEC_ADAPTERS):
 * - Series name: the collection, column set or volume name; series source:
 *   `<adapter>:<path>` (so equal names from two files stay apart; a SeriesRef
 *   with an empty source still finds a name that is unique).
 * - The file is the truth: stored values become Metadata; nothing is derived.
 *   Units stated by the file go into the key `unit:<attr>`.
 * - Re-opening a source that is already registered registers nothing new and
 *   reports the existing series.
 *
 * Adapters register by name with a static Registrar (see AdapterRegistry.h).
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */

#include "Data.h"
#include "Elements.h"

#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace fccvis::adapters {

/** @brief Adapter options by name (values as text, parsed by the adapter). */
using Options = std::map<std::string, std::string>;

/** @brief One option an adapter accepts. */
struct OptionInfo {
  std::string name;          ///< Option name.
  std::string defaultValue;  ///< Default, as text; empty if none.
  std::string description;   ///< What it does.
};

/** @brief What an adapter reads and produces; the source of its help text. */
struct Capabilities {
  std::string description;                  ///< One-line description.
  std::set<data::ElementKind> kinds;        ///< Element kinds produced.
  std::set<std::string> extensions;         ///< Accepted file extensions, with the dot (".csv").
  std::vector<OptionInfo> options;          ///< Accepted options.
  std::vector<std::string> notes;           ///< Sampling and mapping notes.
  std::vector<std::string> limitations;     ///< Known limitations.

  /**
   * @brief Human-readable summary generated from the fields above.
   *
   * Kinds, extensions and options are listed in sorted order, so the text is
   * stable across runs.
   */
  [[nodiscard]] std::string Help(const std::string& name) const;
};

/** @brief A series an adapter registered. */
struct RegisteredSeries {
  data::SeriesRef ref;                     ///< Name and source.
  data::ElementKind kind = data::ElementKind::Point;  ///< Element kind.
  std::size_t entries = 0;                 ///< Number of entries.
  std::set<std::string> metadataKeys;      ///< Metadata keys elements may carry.
  std::string description;                 ///< What the series is.
  bool existing = false;                   ///< Already registered by an earlier Open.
};

/** @brief Something an adapter did not register, and why. */
struct Skipped {
  std::string name;    ///< Collection, row ("line 12") or volume.
  std::string reason;  ///< Why.
};

/** @brief Result of Adapter::Open. */
struct LoadReport {
  std::string source;                     ///< Source label used for the series.
  std::vector<RegisteredSeries> registered;  ///< Registered series.
  std::vector<Skipped> skipped;           ///< Skipped items with reasons.

  /** @brief True if at least one series is available. */
  [[nodiscard]] bool Ok() const { return !registered.empty(); }

  /** @brief Sorts both lists (series by name; skipped by name, then reason). */
  void Sort();

  /**
   * @brief Printable form: one line per series and per skipped item, sorted.
   * @param maxSkipped Skipped items listed before "... and N more".
   */
  [[nodiscard]] std::string ToString(std::size_t maxSkipped = 20) const;
};

/** @brief A source reader. */
class Adapter {
 public:
  virtual ~Adapter() = default;

  /** @brief Registry name ("csv", "detector", ...). */
  [[nodiscard]] virtual std::string Name() const = 0;

  /** @brief What it reads and produces. */
  [[nodiscard]] virtual Capabilities GetCapabilities() const = 0;

  /** @brief Help text generated from GetCapabilities(). */
  [[nodiscard]] std::string Help() const { return GetCapabilities().Help(Name()); }

  /**
   * @brief Reads @p path and registers its series in @p data.
   *
   * Never throws for bad content: problems become LoadReport::skipped
   * entries. An unreadable source gives a report with no series and one
   * skipped entry. Throws std::invalid_argument only for invalid options.
   */
  virtual LoadReport Open(data::SessionData& data, const std::string& path, const Options& options) = 0;
};

}  // namespace fccvis::adapters
