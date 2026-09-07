#include "Session.h"
#include "include/CliOptions.h"
#include <iostream>
#include "GuiApplication.h"

using namespace fccvis::session;
namespace fccvis::cli {
int RunApplication(const CliOptions &options, int& argc, char** argv) {
  auto runSession = new Session(
    "default-visualziation-session",
    options.dataFile,
    options.optionsFile
  );
  if (options.guiRequested) {
#ifdef FCCVIS_ENABLE_GUI
    fccvis::gui::GuiApplication app(argc, argv);
    // Session should manage gui itself.
    app.Run();
    return 0;
#else
    std::cerr << "fccvis: GUI support not compiled in (build with "
                 "-DFCCVIS_ENABLE_GUI=ON)\n";
    return 1;
#endif
  } else {
    std::cout << "[fccvis] Headless render from " << *options.optionsFile
              << '\n';
  }

  return 0;
}

} // namespace fccvis::cli
