/**
 * @file Data.h
 * @brief Format-independent, typed, read-only store of element series.
 *
 * SessionData is addressed by (element type, series name, source). A *series*
 * is an ordered sequence of entries; one entry is the batch of elements of
 * that type for one event or step. SessionData has no knowledge of any input
 * format and holds no cursor: which entry is drawn is decided by the drawing
 * request (see fccvis::scene::meta::Layer).
 *
 * Adapters register series backed by a @ref Series provider. Providers are
 * lazy: an entry is converted once on first access and then served from a
 * bounded per-series cache, so changing a filter never re-reads the source.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include <algorithm>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fccvis::data {

/**
 * @brief Reference to a series, without its element type.
 *
 * The element type is supplied by the typed accessor that uses the reference.
 */
struct SeriesRef {
  /** @brief Series name, chosen by the adapter. */
  std::string name;

  /**
   * @brief Source label (file path, stream name, ...).
   *
   * May be empty when @ref name is unambiguous across sources.
   */
  std::string source;
};

/**
 * @brief Result of resolving a @ref SeriesRef.
 */
enum class Lookup {
  Found,      ///< Exactly one series matches.
  Missing,    ///< No series matches.
  Ambiguous   ///< Several sources provide a series with this type and name.
};

/**
 * @brief Description of a registered series, for introspection and help output.
 */
struct SeriesInfo {
  std::type_index type;     ///< Element type of the series.
  std::string name;         ///< Series name.
  std::string source;       ///< Source label.
  std::string description;  ///< Free-text description provided by the adapter.
  std::size_t entries;      ///< Current number of entries.
};

/**
 * @brief Provider of one series of elements of type @p X.
 *
 * Implemented by adapters. The provider exposes what the source contains and
 * performs no derivation. SessionData serializes all calls to one provider, so
 * implementations need no locking of their own, with one exception: a live
 * source that grows from another thread must synchronize its own growth.
 *
 * @tparam X Element type.
 */
template <typename X>
class Series {
 public:
  virtual ~Series() = default;

  /** @brief Current number of entries. May increase for live sources. */
  [[nodiscard]] virtual std::size_t Size() const = 0;

  /**
   * @brief Loads the elements of one entry.
   *
   * Called at most once per entry while the entry stays cached.
   *
   * @param entry Zero-based entry index, less than @ref Size.
   * @return The elements of the entry.
   */
  [[nodiscard]] virtual std::vector<X> Load(std::size_t entry) const = 0;
};

namespace detail {

/** @brief Type-erased base of a registered series. */
class SeriesSlot {
 public:
  SeriesSlot(std::type_index t, std::string n, std::string s, std::string d)
      : type(t), name(std::move(n)), source(std::move(s)),
        description(std::move(d)) {}
  virtual ~SeriesSlot() = default;

  [[nodiscard]] virtual std::size_t Size() const = 0;

  const std::type_index type;
  const std::string name;
  const std::string source;
  const std::string description;
};

/** @brief Typed slot: provider plus bounded per-entry cache. */
template <typename X>
class TypedSlot final : public SeriesSlot {
 public:
  TypedSlot(std::string n, std::string s, std::string d,
            std::unique_ptr<Series<X>> series, std::size_t capacity)
      : SeriesSlot(std::type_index(typeid(X)), std::move(n), std::move(s),
                   std::move(d)),
        m_series(std::move(series)),
        m_capacity(std::max<std::size_t>(1, capacity)) {}

  [[nodiscard]] std::size_t Size() const override {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_series->Size();
  }

  /**
   * @brief Returns the converted elements of an entry, loading on first use.
   * @return Shared elements, or nullptr if @p entry is out of range.
   */
  [[nodiscard]] std::shared_ptr<const std::vector<X>>
  Get(std::size_t entry) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (entry >= m_series->Size()) return nullptr;

    auto it = m_cache.find(entry);
    if (it != m_cache.end()) return it->second;

    auto loaded =
        std::make_shared<const std::vector<X>>(m_series->Load(entry));
    m_cache.emplace(entry, loaded);
    m_order.push_back(entry);
    while (m_order.size() > m_capacity) {   // FIFO eviction
      m_cache.erase(m_order.front());
      m_order.pop_front();
    }
    return loaded;
  }

 private:
  std::unique_ptr<Series<X>> m_series;
  std::size_t m_capacity;
  mutable std::mutex m_mutex;
  mutable std::unordered_map<std::size_t,
                             std::shared_ptr<const std::vector<X>>> m_cache;
  mutable std::deque<std::size_t> m_order;
};

}  // namespace detail

/**
 * @brief Typed, read-only store of element series.
 *
 * Registration (@ref Add) is a setup-time operation and must not run
 * concurrently with reads. Reads (@ref Entry, @ref Size, @ref Status,
 * @ref List) may run concurrently with each other.
 *
 * Elements returned by @ref Entry are shared and stay valid for as long as the
 * caller holds them, even after cache eviction. Geometry needs no special
 * case: it is a series of a geometry element type with a single entry.
 *
 * SessionData is movable and not copyable.
 */
class SessionData {
 public:
  SessionData() = default;
  SessionData(SessionData&&) noexcept = default;
  SessionData& operator=(SessionData&&) noexcept = default;

  /**
   * @brief Registers a series.
   *
   * @tparam X Element type of the series.
   * @param name         Series name.
   * @param source       Source label; keeps equal names from different files apart.
   * @param series       Provider; must not be null.
   * @param description  Free-text description for introspection.
   * @param cacheEntries Number of converted entries kept in the cache (minimum 1).
   * @return False if @p series is null or (type, name, source) is already registered.
   */
  template <typename X>
  bool Add(std::string name, std::string source,
           std::unique_ptr<Series<X>> series, std::string description = {},
           std::size_t cacheEntries = 4) {
    if (!series) return false;
    auto& slots = m_slots[std::type_index(typeid(X))];
    for (const auto& s : slots)
      if (s->name == name && s->source == source) return false;
    slots.push_back(std::make_unique<detail::TypedSlot<X>>(
        std::move(name), std::move(source), std::move(description),
        std::move(series), cacheEntries));
    return true;
  }

  /**
   * @brief Resolves a series reference.
   * @tparam X Element type.
   */
  template <typename X>
  [[nodiscard]] Lookup Status(const SeriesRef& ref) const {
    Lookup status;
    Locate<X>(ref, status);
    return status;
  }

  /**
   * @brief Number of entries of a series, or 0 if it is not found.
   * @tparam X Element type.
   */
  template <typename X>
  [[nodiscard]] std::size_t Size(const SeriesRef& ref) const {
    Lookup status;
    const auto* slot = Locate<X>(ref, status);
    return slot ? slot->Size() : 0;
  }

  /**
   * @brief Elements of one entry of a series.
   *
   * @tparam X Element type.
   * @return Shared elements, or nullptr if the series is missing or
   *         ambiguous or @p entry is out of range.
   */
  template <typename X>
  [[nodiscard]] std::shared_ptr<const std::vector<X>>
  Entry(const SeriesRef& ref, std::size_t entry) const {
    Lookup status;
    const auto* slot = Locate<X>(ref, status);
    return slot ? static_cast<const detail::TypedSlot<X>*>(slot)->Get(entry)
                : nullptr;
  }

  /** @brief All registered series, sorted by name and source. */
  [[nodiscard]] std::vector<SeriesInfo> List() const {
    std::vector<SeriesInfo> out;
    for (const auto& [type, slots] : m_slots)
      for (const auto& s : slots)
        out.push_back({s->type, s->name, s->source, s->description, s->Size()});
    std::sort(out.begin(), out.end(),
              [](const SeriesInfo& a, const SeriesInfo& b) {
                return std::tie(a.name, a.source) < std::tie(b.name, b.source);
              });
    return out;
  }

  /** @brief Registered series of element type @p X, sorted by name and source. */
  template <typename X>
  [[nodiscard]] std::vector<SeriesInfo> List() const {
    std::vector<SeriesInfo> out;
    const std::type_index t(typeid(X));
    for (const auto& info : List())
      if (info.type == t) out.push_back(info);
    return out;
  }

  /** @brief True if no series is registered. */
  [[nodiscard]] bool Empty() const noexcept { return m_slots.empty(); }

  /** @brief Removes every series. */
  void Clear() noexcept { m_slots.clear(); }

 private:
  template <typename X>
  const detail::SeriesSlot* Locate(const SeriesRef& ref, Lookup& status) const {
    const detail::SeriesSlot* found = nullptr;
    int matches = 0;
    auto it = m_slots.find(std::type_index(typeid(X)));
    if (it != m_slots.end()) {
      for (const auto& s : it->second) {
        if (s->name == ref.name && (ref.source.empty() || s->source == ref.source)) {
          found = s.get();
          ++matches;
        }
      }
    }
    status = matches == 0 ? Lookup::Missing
             : matches == 1 ? Lookup::Found
                            : Lookup::Ambiguous;
    return matches == 1 ? found : nullptr;
  }

  std::unordered_map<std::type_index,
                     std::vector<std::unique_ptr<detail::SeriesSlot>>>
      m_slots;
};

}  // namespace fccvis::session
