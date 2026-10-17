/**
 * @file Script.h
 * @brief An ordered list of changes run as one batch through Session::Resolve.
 *
 * A Script is a value: copying it shares the (immutable) changes. Running it
 * is one Session::Resolve batch: changes apply in order, the batch stops at
 * the first failing change, backends are notified once at the end
 * (ARCHITECTURE decision 10).
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include "Changes.h"
#include "Session.h"

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace fccvis::api {

/** @brief Ordered changes, run as one batch. */
class Script {
 public:
  Script() = default;

  /** @brief Appends a change (shared, never modified). */
  Script& Add(std::shared_ptr<const changes::Change> change) {
    if (change) m_changes.push_back(std::move(change));
    return *this;
  }

  /** @brief Appends a change given by value, for example `Add(changes::CreateScene{"xy"})`. */
  template <typename C>
    requires std::is_base_of_v<changes::Change, std::decay_t<C>>
  Script& Add(C&& change) {
    return Add(std::make_shared<const std::decay_t<C>>(std::forward<C>(change)));
  }

  /** @brief Appends every change of @p other. */
  Script& Append(const Script& other) {
    m_changes.insert(m_changes.end(), other.m_changes.begin(), other.m_changes.end());
    return *this;
  }

  /** @brief Number of changes. */
  [[nodiscard]] std::size_t Size() const { return m_changes.size(); }
  /** @brief True if there is nothing to run. */
  [[nodiscard]] bool Empty() const { return m_changes.empty(); }
  /** @brief The changes, in order. */
  [[nodiscard]] const std::vector<std::shared_ptr<const changes::Change>>& Changes() const { return m_changes; }

  /** @brief One Describe() line per change. */
  [[nodiscard]] std::vector<std::string> Describe() const {
    std::vector<std::string> out;
    for (const auto& c : m_changes) out.push_back(c->Describe());
    return out;
  }

  /**
   * @brief Runs the script as one batch.
   * @param session Session to modify.
   * @param origin Backend id for interaction-driven scripts; empty for user scripts.
   */
  changes::Result Run(session::Session& session, const std::string& origin = {}) const {
    std::vector<const changes::Change*> batch;
    batch.reserve(m_changes.size());
    for (const auto& c : m_changes) batch.push_back(c.get());
    return session.Resolve(std::span<const changes::Change* const>(batch), origin);
  }

 private:
  std::vector<std::shared_ptr<const changes::Change>> m_changes;
};

/** @brief Runs @p script on @p session (same as script.Run(session)). */
inline changes::Result Run(session::Session& session, const Script& script) { return script.Run(session); }

}  // namespace fccvis::api
