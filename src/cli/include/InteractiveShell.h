#pragma once
#include <string>
#include <vector>
#include "Session.h"
#include "src/backend/root/include/RootSceneManager.h"

namespace fccvis::cli {

class InteractiveShell {
public:
  explicit InteractiveShell(fccvis::session::Session& session);
  void Run();

private:
  fccvis::session::Session& m_session;
  fccvis::backend::root::RootSceneManager m_sceneManager;
  std::vector<std::string> m_commandHistory;
  bool m_quitRequested = false;

  void EnsureEveManager();
  void BindSessionIntoCling();
  bool HandleBuiltin(const std::string& line);

  void Quit();
  void Save(const std::string& path);
  void See(const std::string& sceneName);
  void Close(const std::string& sceneName);
  void ListScenes();
  void ListOpen();
  void Help();
};

} // namespace fccvis::cli
