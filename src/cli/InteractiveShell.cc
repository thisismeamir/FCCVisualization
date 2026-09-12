#include "InteractiveShell.h"
#include <TEveManager.h>
#include <TInterpreter.h>
#include <TSystem.h>
#include <TROOT.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include "fccvis/FccvisConfig.h"
#include <TApplication.h>



namespace fccvis::cli {

InteractiveShell::InteractiveShell(fccvis::session::Session& session)
    : m_session(session), m_sceneManager(session) {
  EnsureEveManager();
  BindSessionIntoCling();
}

void InteractiveShell::EnsureEveManager() {
  if (!gApplication) {
    int argc = 0;
    char** argv = nullptr;
    new TApplication("fccvis_headless", &argc, argv);
  }
  if (!gEve) {
    TEveManager::Create(kFALSE); // map_window=false — no visible browser chrome, but a real display exists
  }
}

void InteractiveShell::BindSessionIntoCling() {
  gInterpreter->AddIncludePath(FCCVIS_UNIFIED_INCLUDE_DIR);
  gInterpreter->AddIncludePath(FCCVIS_UNIFIED_INCLUDE_DIR "/fccvis");
  gInterpreter->Declare("#include <fccvis/Session.h>");

  gSystem->Load(FCCVIS_BUILD_LIB_DIR "/libFCCVisualization.so");

  gInterpreter->ProcessLine(
      Form("fccvis::session::Session* session = "
           "(fccvis::session::Session*)%p;",
           (void*)&m_session));
}

void InteractiveShell::Run() {
  std::string line;
  std::cout << "fccvis> ";
  while (!m_quitRequested && std::getline(std::cin, line)) {
    if (line.empty()) {
      std::cout << "fccvis> ";
      continue;
    }
    if (!HandleBuiltin(line)) {
      m_commandHistory.push_back(line);
      gInterpreter->ProcessLine(line.c_str());
    }
    if (!m_quitRequested) {
      std::cout << "fccvis> ";
    }
  }
}

bool InteractiveShell::HandleBuiltin(const std::string& line) {
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

void InteractiveShell::Quit() {
  m_quitRequested = true;
}

void InteractiveShell::Save(const std::string& path) {
  if (path.empty()) {
    std::cerr << "fccvis: .save requires a path\n";
    return;
  }
  std::ofstream out(path);
  if (!out) {
    std::cerr << "fccvis: could not open '" << path << "' for writing\n";
    return;
  }
  const auto& optionsFile = m_session.GetOptionsFile();
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
  for (const auto& c : m_commandHistory) {
    out << c << "\n";
  }
}

void InteractiveShell::See(const std::string& name) {
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

void InteractiveShell::Close(const std::string& name) {
  if (name.empty()) {
    std::cerr << "fccvis: .close requires a scene name\n";
    return;
  }
  m_sceneManager.Close(name);
}

void InteractiveShell::ListScenes() {
  for (const auto& name : m_sceneManager.SceneNames()) { // your new accessor
    std::cout << (m_sceneManager.IsOpen(*name) ? "* " : "  ") << name << "\n";
  }
}

void InteractiveShell::ListOpen() {
  for (const auto& scene : m_sceneManager.OpenScenes()) { // your new accessor
    std::cout << scene->name << "\n"; // adjust to whatever RootScene actually exposes
  }
}

void InteractiveShell::Help() {
  std::cout <<
    ".see <name>    open or refresh a scene\n" <<
    ".close <name>  close an open scene\n" <<
    ".scenes        list all known scenes ('*' = open)\n" <<
    ".open          list currently open scenes\n" <<
    ".save <path>   write session to a new options.cpp\n" <<
    ".quit / .q     exit\n";
}

} // namespace fccvis::cli
