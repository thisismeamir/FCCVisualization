/**
 * @file GuiApplication.h
 * @brief ROOT GUI application lifecycle management.
 *
 * Defines the @ref fccvis::gui::GuiApplication class, which owns and
 * manages the ROOT @c TApplication instance used by the
 * FCCVisualization graphical interface.
 *
 * The GUI application is associated with an existing
 * @ref fccvis::session::Session and provides the main event loop and
 * termination interface for the graphical application.
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

#include "Session.h"

#include <TApplication.h>

#include <memory>

/**
 * @brief Graphical user interface components.
 *
 * Contains the objects responsible for managing the FCCVisualization
 * graphical application and its interaction with the ROOT GUI
 * framework.
 */
namespace fccvis::gui {

/**
 * @brief Manages the ROOT GUI application for FCCVisualization.
 *
 * GuiApplication owns the ROOT @c TApplication instance responsible
 * for managing the graphical application's event loop.
 *
 * The application operates on an existing
 * @ref fccvis::session::Session, which is referenced rather than owned
 * by the GUI application. The lifetime of the Session must therefore
 * extend beyond the lifetime of the GuiApplication.
 *
 * @see fccvis::session::Session
 * @see TApplication
 */
class GuiApplication {
public:
  /**
   * @brief Construct a GUI application.
   *
   * Creates the underlying ROOT @c TApplication using the supplied
   * command-line arguments and associates the application with an
   * existing visualization session.
   *
   * @param argc Reference to the application's command-line argument
   *             count.
   * @param argv Command-line arguments passed to the ROOT application.
   * @param session Session whose data and configuration are visualized.
   *
   * @note The session is not owned by GuiApplication.
   */
  GuiApplication(int& argc,
                 char** argv,
                 fccvis::session::Session& session);

  /**
   * @brief Destroy the GUI application.
   *
   * Releases the owned ROOT @c TApplication instance. The associated
   * Session is not destroyed because GuiApplication does not own it.
   */
  ~GuiApplication();

  /**
   * @brief Start the GUI event loop.
   *
   * Runs the underlying ROOT application and processes graphical
   * events until the application is terminated.
   */
  void Run();

  /**
   * @brief Request termination of the GUI application.
   *
   * Stops the graphical application's event loop.
   */
  void Terminate();

private:
  /**
   * @brief ROOT application instance owned by the GUI application.
   *
   * The @c TApplication manages the ROOT graphical application
   * lifecycle and event processing.
   */
  std::unique_ptr<TApplication> m_application;

  /**
   * @brief Session visualized by this application.
   *
   * This is a non-owning reference. The referenced Session must
   * remain alive for the lifetime of the GuiApplication.
   */
  fccvis::session::Session& m_session;
};

} // namespace fccvis::gui
