#include "Session.h"
#include <TApplication.h>
#include <memory>

namespace fccvis::gui {

class GuiApplication {
  public:
  GuiApplication(int& argc, char** argv, fccvis::session::Session& session);
  ~GuiApplication();

  void Run();
  void Terminate();
  private:
  std::unique_ptr<TApplication> m_application;
  fccvis::session::Session& m_session;
};
}
