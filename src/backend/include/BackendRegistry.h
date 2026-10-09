#pragma once
/**
 * @file BackendRegistry.h
 * @brief Compile-time registry of backends by name.
 *
 * Each backend .cc registers itself with a static Registrar, for example
 * `static backend::Registrar reg("root", [] { return std::make_unique<RootBackend>(); });`.
 * The backend libraries must be OBJECT libraries or linked whole-archive, or
 * the linker drops the registrar.
 */

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Backend.h"

namespace fccvis::backend {

/** @brief Name-to-factory table of the backends compiled into the build. */
class Registry {
 public:
  using Factory = std::function<std::unique_ptr<Backend>()>;

  /** @brief The process-wide registry. */
  static Registry& Global() {
    static Registry instance;
    return instance;
  }

  /** @brief Registers a factory. @return False if the name is taken. */
  bool Register(std::string name, Factory factory) {
    return m_factories.emplace(std::move(name), std::move(factory)).second;
  }

  /** @brief New instance of backend @p name, or nullptr if unknown. */
  [[nodiscard]] std::unique_ptr<Backend> Create(const std::string& name) const {
    auto it = m_factories.find(name);
    return it == m_factories.end() ? nullptr : it->second();
  }

  /** @brief Registered backend names, sorted. */
  [[nodiscard]] std::vector<std::string> Names() const {
    std::vector<std::string> out;
    for (const auto& [name, f] : m_factories) out.push_back(name);
    return out;
  }

  /** @brief Generated help text of a backend, or an empty string if unknown. */
  [[nodiscard]] std::string Help(const std::string& name) const {
    auto b = Create(name);
    return b ? b->GetCapabilities().Help(name) : std::string{};
  }

 private:
  std::map<std::string, Factory> m_factories;
};

/** @brief Registers a backend at static initialization. */
struct Registrar {
  Registrar(std::string name, Registry::Factory factory) {
    Registry::Global().Register(std::move(name), std::move(factory));
  }
};

}  // namespace fccvis::backend
