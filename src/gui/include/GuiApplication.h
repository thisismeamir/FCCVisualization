#include <TApplication.h>
#include <memory>

namespace fccvis::gui {

class GuiApplication {
  public:
  GuiApplication(int& argc, char** argv);
  ~GuiApplication();

  void Run();
  private:
  std::unique_ptr<TApplication> m_application;
};
}
