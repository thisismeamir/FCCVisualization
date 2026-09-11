#include "InteractiveShell.h"
#include <TInterpreter.h>
#include <fstream>
#include <iostream>
#include "fccvis/FccvisConfig.h"

namespace fccvis::cli {
InteractiveShell::InteractiveShell(fccvis::session::Session &session)
    : m_session(session) {
  BindSessionIntoCling();
}

void InteractiveShell::BindSessionIntoCling() {
  gInterpreter->AddIncludePath(FCCVIS_UNIFIED_INCLUDE_DIR);
  gInterpreter->AddIncludePath(FCCVIS_UNIFIED_INCLUDE_DIR "/fccvis");
  gInterpreter->Declare("#include <fccvis/Session.h>");
  // Classic ROOT pointer-injection trick: expose the live Session& as a
  // named symbol inside the interpreter, so raw ConfigureSession-style
  // snippets typed at the prompt operate on the SAME object main.cpp holds.
  gInterpreter->ProcessLine(Form("fccvis::session::Session* session = "
                                 "(fccvis::session::Session*)%p;",
                                 (void *)&m_session));
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
      // Fall through to Cling for arbitrary C++: e.g.
      // session->GetOptions().cameras["cam1"]->positionVector = {0,0,10};
      gInterpreter->ProcessLine(line.c_str());
    }
    if (!m_quitRequested) {
      std::cout << "fccvis> ";
    }
  }
}
bool InteractiveShell::HandleBuiltin(const std::string &line) {
  if (line.empty() || line[0] != '.') {
    return false; // not a builtin — hand off to Cling untouched
  }

  std::istringstream iss(line.substr(1)); // strip the leading '.'
  std::string cmd;
  iss >> cmd;

  if (cmd == "quit" || cmd == "q") {
    Quit();
    return true;
  }
  if (cmd == "save") {
    std::string path;
    iss >> path;
    Save(path);
    return true;
  }
  if (cmd == "see") {
    std::string name;
    iss >> name;
    See(name);
    return true;
  }

  std::cerr << "fccvis: unknown shell command '." << cmd << "'\n";
  return true; // consumed — don't let an unknown '.' fall through to Cling
}

// TODO: See functionality to address opening a scene or layout
void InteractiveShell::See(const std::string& sceneOrLayoutName) {
  std::cout << "See Option for: " << sceneOrLayoutName << std::endl;
};

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

  // Original options.cpp content, verbatim, if the session was loaded from one.
  const auto& optionsFile = m_session.GetOptionsFile();
  if (optionsFile) {
    std::ifstream in(*optionsFile);
    if (in) {
      out << in.rdbuf();
      out << "\n";
    } else {
      std::cerr << "fccvis: warning — could not reopen original options file '"
                 << *optionsFile << "', continuing with commands only\n";
    }
  }

  out << "// --- commands appended by interactive session ---\n";
  for (const auto& cmd : m_commandHistory) {
    out << cmd << "\n";
  }
}
} // namespace fccvis::cli
