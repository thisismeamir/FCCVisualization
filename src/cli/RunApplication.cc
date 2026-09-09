#include "GuiApplication.h"
#include "InteractiveShell.h"
#include "Session.h"
#include "include/CliOptions.h"
#include <thread>

using namespace fccvis::session;
namespace fccvis::cli {

int RunApplication(const CliOptions &options, int &argc, char **argv) {
  fccvis::session::Session session(
      options.dataFile ? options.dataFile->stem().string() : "untitled",
      options.dataFile, options.optionsFile);

  const bool wantsShell = options.interactive;

  if (options.guiRequested) {
#ifdef FCCVIS_ENABLE_GUI
    fccvis::gui::GuiApplication app(argc, argv, session);

    if (!wantsShell) {
      app.Run(); // Case 1: blocking event loop, no shell
      return 0;
    }

    // Case 2: GUI event loop on its own thread, shell owns main thread
    std::thread guiThread([&app]() { app.Run(); });

    fccvis::cli::InteractiveShell shell(session);
    shell.Run();

    guiThread.join(); // window close already terminates the app
    return 0;
#else
    std::cerr << "fccvis: GUI support not compiled in (build with "
                 "-DFCCVIS_ENABLE_GUI=ON)\n";
    return 1;
#endif
  }

  // Headless path
  if (wantsShell) {
    fccvis::cli::InteractiveShell shell(session); // Case 5
    shell.Run();
    return 0;
  }

  return 0; // Case 4: whatever ConfigureSession did, that's the output
}

} // namespace fccvis::cli
