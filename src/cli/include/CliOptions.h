/**
 * @file CliOptions.h
 * @brief Command-line options and argument handling for FCCVisualization.
 *
 * Defines the command-line configuration used to control application
 * startup, including GUI mode, interactive mode, and optional input
 * files.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 *
 * @copyright Copyright 2026 FCC Project at CERN
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <filesystem>
#include <optional>

/**
 * @brief Command-line interface functionality.
 *
 * Contains the command-line configuration and argument-processing
 * functions used to initialize FCCVisualization.
 */
namespace fccvis::cli {

/**
 * @brief Command-line options for the FCCVisualization application.
 *
 * CliOptions represents the configuration produced by parsing the
 * application's command-line arguments.
 *
 * The options determine which application interface is started and
 * which optional input files are supplied to the session.
 */
struct CliOptions {
  /**
   * @brief Whether the graphical user interface was requested.
   *
   * If @c true, the application starts its graphical interface.
   */
  bool guiRequested = false;

  /**
   * @brief Whether interactive mode is enabled.
   *
   * Interactive mode controls whether the application provides an
   * interactive command interface after initialization.
   */
  bool interactive = true;

  /**
   * @brief Optional path to the event-data file.
   *
   * If present, the path identifies the input file containing the
   * event data to be loaded into the visualization session.
   */
  std::optional<std::filesystem::path> dataFile;

  /**
   * @brief Optional path to the visualization options file.
   *
   * If present, the path identifies the configuration file used to
   * initialize the session's visualization options.
   */
  std::optional<std::filesystem::path> optionsFile;
};

/**
 * @brief Parse application command-line arguments.
 *
 * Converts the raw command-line arguments into a structured
 * @ref CliOptions object.
 *
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line argument strings.
 *
 * @return Parsed command-line options.
 */
CliOptions ParseArgs(int argc, char** argv);

/**
 * @brief Validate parsed command-line options.
 *
 * Checks whether the supplied combination of command-line options
 * is valid for starting the application.
 *
 * @param opts Parsed command-line options to validate.
 *
 * @throws An implementation-defined exception if the supplied
 *         configuration is invalid.
 */
void ValidateArgs(const CliOptions& opts);

} // namespace fccvis::cli
