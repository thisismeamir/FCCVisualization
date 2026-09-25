/**
 * @file InteractiveShell.h
 * @brief Interactive command-line shell for FCCVisualization.
 *
 * Defines the interactive shell used to inspect and control an
 * FCCVisualization session from the command line.
 *
 * The shell connects the active session to the ROOT Cling interpreter
 * and provides built-in commands for navigating and controlling
 * visualization scenes.
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
#include "src/backend/root/include/RootSceneManager.h"

#include <string>
#include <vector>

/**
 * @brief Command-line interface functionality.
 *
 * Contains the interactive shell and other command-line application
 * components used by FCCVisualization.
 */
namespace fccvis::cli {

/**
 * @brief Interactive command-line shell for a visualization session.
 *
 * InteractiveShell provides a command interface for inspecting and
 * controlling an active @ref fccvis::session::Session.
 *
 * The shell maintains command history, provides built-in commands for
 * scene management, and exposes the active session to the ROOT Cling
 * interpreter.
 *
 * The shell uses a @ref fccvis::backend::root::RootSceneManager to
 * manage the ROOT-based visualization of scenes.
 *
 * @note The Session is referenced but not owned by the shell. The
 *       referenced session must remain valid for the lifetime of the
 *       InteractiveShell.
 *
 * @see fccvis::session::Session
 * @see fccvis::backend::root::RootSceneManager
 */
class InteractiveShell {
public:
  /**
   * @brief Construct an interactive shell for a session.
   *
   * @param session Session controlled by the interactive shell.
   *
   * @note The session is not owned by InteractiveShell.
   */
  explicit InteractiveShell(fccvis::session::Session& session);

  /**
   * @brief Run the interactive shell.
   *
   * Starts processing interactive input until the user requests
   * termination.
   */
  void Run();

  /**
  * @brief Processes one line of shell.
  */
  void ProcessOneLine();


private:
  /**
   * @brief Session controlled by the shell.
   *
   * This is a non-owning reference to the active visualization
   * session.
   */
  fccvis::session::Session& m_session;

  /**
   * @brief ROOT scene manager used by the shell.
   *
   * Handles the ROOT-specific creation and management of visualized
   * scenes.
   */
  fccvis::backend::root::RootSceneManager m_sceneManager;

  /**
   * @brief History of commands entered by the user.
   *
   * Commands are stored in the order in which they were entered.
   */
  std::vector<std::string> m_commandHistory;

  /**
   * @brief Whether the shell has received a quit request.
   *
   * The shell uses this flag to terminate its command-processing
   * loop.
   */
  bool m_quitRequested = false;

  /**
   * @brief Ensure that the ROOT Eve manager is initialized.
   *
   * Initializes the ROOT Eve graphical infrastructure required by
   * the scene manager if it has not already been created.
   */
  void EnsureEveManager();

  /**
   * @brief Bind the active session into the Cling interpreter.
   *
   * Makes the session available to interactive commands executed
   * through ROOT Cling.
   */
  void BindSessionIntoCling();

  /**
   * @brief Handle a built-in shell command.
   *
   * Determines whether the supplied command corresponds to one of
   * the shell's built-in commands and executes it when applicable.
   *
   * @param line Input command line.
   *
   * @return @c true if the command was handled as a built-in command;
   *         @c false if it should be passed to the interactive
   *         interpreter.
   */
  bool HandleBuiltin(const std::string& line);

  /**
   * @brief Request termination of the interactive shell.
   */
  void Quit();

  /**
   * @brief Save the current visualization state.
   *
   * @param path Destination path for the saved state.
   */
  void Save(const std::string& path);

  /**
   * @brief Display a named scene.
   *
   * @param sceneName Name of the scene to display.
   */
  void See(const std::string& sceneName);

  /**
   * @brief Close a named scene.
   *
   * @param sceneName Name of the scene to close.
   */
  void Close(const std::string& sceneName);

  /**
   * @brief List the scenes available in the session.
   */
  void ListScenes();

  /**
   * @brief List scenes currently open in the visualization.
   */
  void ListOpen();

  /**
   * @brief Display help for available shell commands.
   */
  void Help();
};


class StdinHandler : public TFileHandler {
public:
  explicit StdinHandler(fccvis::cli::InteractiveShell &shell)
      : TFileHandler(0, TFileHandler::kRead), m_shell(shell) {}

  Bool_t ReadNotify() override {
    m_shell.ProcessOneLine();
    return kTRUE;
  }

private:
  fccvis::cli::InteractiveShell &m_shell;
};

} // namespace fccvis::cli
