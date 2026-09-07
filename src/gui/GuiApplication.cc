#include "GuiApplication.h"
#include <TEveManager.h>

namespace fccvis::gui {

GuiApplication::GuiApplication(int& argc, char** argv)
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

} // namespace fccvis::gui
