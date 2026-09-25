#include "InteractiveShell.h"
#include "fccvis/FccvisConfig.h"
#include "src/backend/root/include/RootSceneManager.h"
#include <TApplication.h>
#include <TEveManager.h>
#include <TInterpreter.h>
#include <TROOT.h>
#include <TSystem.h>
#include <fstream>
#include <iostream>
#include <sstream>

namespace fccvis::cli {

InteractiveShell::InteractiveShell(fccvis::session::Session &session)
    : m_session(session), m_sceneManager(session) {
  EnsureEveManager();
  BindSessionIntoCling();
}

void InteractiveShell::EnsureEveManager() {
  if (!gApplication) {
    int argc = 0;
    char **argv = nullptr;
    new TApplication("fccvis_headless", &argc, argv);
  }
  if (!gEve) {
    TEveManager::Create(kTRUE, "FI");
  }
}

void InteractiveShell::BindSessionIntoCling() {
  gInterpreter->Declare("#include <fccvis/Session.h>");
  gInterpreter->Declare("#include <fccvis/CollectionVisualizer.h>");
  gInterpreter->Declare("#include <fccvis/RootSceneManager.h>");

  gSystem->Load("libFCCVisualization.so");
  gInterpreter->ProcessLine(Form("fccvis::session::Session* session = "
                                 "(fccvis::session::Session*)%p;",
                                 (void *)&m_session));
  gInterpreter->ProcessLine(Form(
    "fccvis::backend::root::RootSceneManager* backend = "
    "(fccvis::backend::root::RootSceneManager*) %p;",
    (void *) &m_sceneManager
  ));
  // Making sure that if the user had previous saved data we load it
  if (const auto &optionsFile = m_session.GetOptionsFile()) {
    gInterpreter->LoadFile(optionsFile->c_str());
  }
}

void InteractiveShell::Run() {
std::cout << "fccvis> " << std::flush;
auto handler = std::make_unique<StdinHandler>(*this);
handler->Add();
gApplication->Run(kTRUE); // returns once Terminate() is called
}


bool InteractiveShell::HandleBuiltin(const std::string &line) {
  if (line.empty() || line[0] != '.') {
    return false;
  }

  std::istringstream iss(line.substr(1));
  std::string cmd;
  iss >> cmd;

  if (cmd == "quit" || cmd == "q") {
    Quit();
  } else if (cmd == "save") {
    std::string path;
    iss >> path;
    Save(path);
  } else if (cmd == "see") {
    std::string name;
    iss >> name;
    See(name);
  } else if (cmd == "close") {
    std::string name;
    iss >> name;
    Close(name);
  } else if (cmd == "scenes") {
    ListScenes();
  } else if (cmd == "open") {
    ListOpen();
  } else if (cmd == "help") {
    Help();
  } else {
    std::cerr << "fccvis: unknown shell command '." << cmd << "'\n";
  }
  return true;
}

void InteractiveShell::ProcessOneLine() {
  std::string line;
  if (!std::getline(std::cin, line)) { // EOF (Ctrl-D)
    Quit();
    return;
  }
  if (!line.empty()) {
    if (!HandleBuiltin(line)) {
      m_commandHistory.push_back(line);
      gInterpreter->ProcessLine(line.c_str());
    }
  }
  if (!m_quitRequested) {
    std::cout << "fccvis> " << std::flush;
  }
}

void InteractiveShell::Quit() {
  m_quitRequested = true;
  m_sceneManager.CloseAll();
  if(gApplication) gApplication->Terminate();
}

void InteractiveShell::Save(const std::string &path) {
  if (path.empty()) {
    std::cerr << "fccvis: .save requires a path\n";
    return;
  }
  std::ofstream out(path);
  if (!out) {
    std::cerr << "fccvis: could not open '" << path << "' for writing\n";
    return;
  }
  const auto &optionsFile = m_session.GetOptionsFile();
  if (optionsFile) {
    std::ifstream in(*optionsFile);
    if (in) {
      out << in.rdbuf() << "\n";
    } else {
      std::cerr << "fccvis: warning — could not reopen original options file '"
                << *optionsFile << "', continuing with commands only\n";
    }
  }
  out << "// --- commands appended by interactive session ---\n";
  for (const auto &c : m_commandHistory) {
    out << c << "\n";
  }
}

void InteractiveShell::See(const std::string &name) {
  if (name.empty()) {
    std::cerr << "fccvis: .see requires a scene name\n";
    return;
  }
  if (m_sceneManager.IsOpen(name)) {
    m_sceneManager.Update(name);
    return;
  }
  if (!m_sceneManager.Open(name)) {
    std::cerr << "fccvis: could not open scene '" << name << "'\n";
  }
}

void InteractiveShell::Close(const std::string &name) {
  if (name.empty()) {
    std::cerr << "fccvis: .close requires a scene name\n";
    return;
  }
  m_sceneManager.Close(name);
}

void InteractiveShell::ListScenes() {
  for (const auto &name : m_sceneManager.SceneNames()) { // your new accessor
    std::cout << (m_sceneManager.IsOpen(*name) ? "* " : "  ") << *name << "\n";
  }
}

void InteractiveShell::ListOpen() {
  for (const auto &scene : m_sceneManager.OpenScenes()) { // your new accessor
    std::cout << scene->name
              << "\n"; // adjust to whatever RootScene actually exposes
  }
}

void InteractiveShell::Help() {
  std::cout << ".*see <name>    open or refresh a scene\n"
            << ".close <name>  close an open scene\n"
            << ".scenes        list all known scenes ('*' = open)\n"
            << ".open          list currently open scenes\n"
            << ".save <path>   write session to a new options.cpp\n"
            << ".quit / .q     exit\n";
}

} // namespace fccvis::cli
