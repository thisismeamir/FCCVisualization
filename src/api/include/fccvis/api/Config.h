/**
 * @file Config.h
 * @brief Session configuration files: the commands that rebuild a session.
 *
 * A configuration is JSON:
 * @code{.json}
 * {
 *   "format": "fccvis-config",
 *   "version": 1,
 *   "commands": ["load csv /data/hits.csv entry=event", "scene new xy"]
 * }
 * @endcode
 * The commands are shell commands (the interpreter's journal of
 * state-changing commands plus the final camera of every scene). Loading
 * replays them through the interpreter, so every change goes through
 * Session::Resolve. Data is not stored: only the sources and adapter options
 * in the `load` commands. Backends are not stored.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace fccvis::api {

/** @brief Configuration format version written by this build. */
inline constexpr int kConfigVersion = 1;

/** @brief A configuration that cannot be read. */
class ConfigError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

/** @brief Configuration JSON for @p commands. */
[[nodiscard]] std::string WriteConfig(const std::vector<std::string>& commands);

/**
 * @brief Commands of a configuration.
 * @throws ConfigError for malformed JSON (with line and column), another
 *         format, a newer version or a missing command list.
 */
[[nodiscard]] std::vector<std::string> ReadConfig(std::string_view json);

/** @brief True if @p text looks like a configuration (a JSON object), not a script. */
[[nodiscard]] bool IsConfig(std::string_view text);

}  // namespace fccvis::api
