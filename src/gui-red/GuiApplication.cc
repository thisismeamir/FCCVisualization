#include "GuiApplication.h"
#include "Session.h"
#include <TEveManager.h>
#include <memory>
#include <utility>

namespace fccvis::gui {

GuiApplication::GuiApplication(int& argc, char** argv, fccvis::session::Session& session)
  : m_session(session)
{

    m_application = std::make_unique<TApplication>("fccvis", &argc, argv);
    TEveManager::Create(kTRUE, "FI");
}

GuiApplication::~GuiApplication()
{
    if (gEve)
        TEveManager::Terminate();
}

void GuiApplication::Run()
{
    m_application->Run();
}
void GuiApplication::Terminate(){
  m_application->Terminate();
}
} // namespace fccvis::gui
