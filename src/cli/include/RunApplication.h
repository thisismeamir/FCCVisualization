/**
 * @file RunApplication.h
 * @brief Application-level entry point for FCCVisualization.
 *
 * Declares the function responsible for starting FCCVisualization
 * from an already parsed command-line configuration.
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

#include "CliOptions.h"

/**
 * @brief Command-line interface functionality.
 */
namespace fccvis::cli {

/**
 * @brief Run the FCCVisualization application.
 *
 * Starts the application using the supplied command-line options and
 * process arguments.
 *
 * @param options Parsed and validated command-line options.
 * @param argc Reference to the process argument count.
 * @param argv Process argument array.
 *
 * @return Application exit status.
 */
int RunApplication(const CliOptions& options, int& argc, char** argv);

} // namespace fccvis::cli
