#include "include/CliOptions.h"
#include <iostream>

namespace fccvis::cli {
int RunApplication(const CliOptions &options) {
  if (options.guiRequested) {
#ifdef FCC_ENABLE_GUI
    fccvis::gui::GuiApplication app(argc, argv);)
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
