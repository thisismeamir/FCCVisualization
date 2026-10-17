#pragma once
/**
 * @file AdapterRegistry.h
 * @brief Compile-time registry of adapters by name.
 *
 * Each adapter .cc registers itself with a static Registrar, for example
 * `static adapters::Registrar reg("csv", [] { return std::make_unique<CsvAdapter>(); });`.
 * Disabled adapters (FCCVIS_ENABLE_<NAME>=OFF) are not compiled, so they are
 * not registered.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */

#include "Adapter.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace fccvis::adapters {

/** @brief Name-to-factory table of the adapters compiled into the build. */
class Registry {
 public:
  /** @brief Creates a new adapter instance. */
  using Factory = std::function<std::unique_ptr<Adapter>()>;

  /** @brief The process-wide registry. */
  static Registry& Global() {
    static Registry instance;
    return instance;
  }

  /** @brief Registers a factory. @return False if the name is taken. */
  bool Register(std::string name, Factory factory) {
    return m_factories.emplace(std::move(name), std::move(factory)).second;
  }

  /** @brief New instance of adapter @p name, or nullptr if unknown. */
  [[nodiscard]] std::unique_ptr<Adapter> Create(const std::string& name) const {
    auto it = m_factories.find(name);
    return it == m_factories.end() ? nullptr : it->second();
  }

  /** @brief Registered adapter names, sorted. */
  [[nodiscard]] std::vector<std::string> Names() const {
    std::vector<std::string> out;
    for (const auto& [name, f] : m_factories) out.push_back(name);
    return out;
  }

  /** @brief Help text of an adapter, or an empty string if unknown. */
  [[nodiscard]] std::string Help(const std::string& name) const {
    auto a = Create(name);
    return a ? a->Help() : std::string{};
  }

  /**
   * @brief Adapter whose Capabilities::extensions contain the extension of
   *        @p path (case-insensitive), or nullptr. First match in name order.
   */
  [[nodiscard]] std::unique_ptr<Adapter> ForPath(const std::string& path) const;

 private:
  std::map<std::string, Factory> m_factories;
};

/** @brief Registers an adapter at static initialization. */
struct Registrar {
  /** @brief Registers @p factory under @p name in Registry::Global(). */
  Registrar(std::string name, Registry::Factory factory) {
    Registry::Global().Register(std::move(name), std::move(factory));
  }
};

}  // namespace fccvis::adapters
