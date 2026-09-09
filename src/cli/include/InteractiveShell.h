#pragma once
#include <string>
#include <vector>
#include "Session.h"

namespace fccvis::cli {

class InteractiveShell {
public:
  explicit InteractiveShell(fccvis::session::Session& session);
  void Run(); // blocking REPL loop

private:
  fccvis::session::Session& m_session;
  bool m_quitRequested = false;
  std::vector<std::string> m_commandHistory;
  void BindSessionIntoCling();
  bool HandleBuiltin(const std::string& line); // true if consumed
  void Quit();
  void Save(const std::string& path);
  void See(const std::string& sceneOrLayoutName);
};

} // namespace fccvis::cli
