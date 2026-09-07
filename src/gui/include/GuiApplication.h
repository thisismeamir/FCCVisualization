#include <memory>
#include <TApplication.h>

namespace fccvis::gui {

class GuiApplication {
  public:
  TApplication GetApplication(int& argc, char** argv);
  ~GetApplication();

  void Run();
  private:
  std::unique_ptr<TApplication> m_application;
};
}
