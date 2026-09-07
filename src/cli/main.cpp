#include "include/CliOptions.h"
#include "include/RunApplication.h"
#include <iostream>

using namespace fccvis::cli;
int main(int argc, char **argv) {
  try {
    const CliOptions options = ParseArgs(argc, argv);
    ValidateArgs(options);
    return RunApplication(options, argc, argv);
  } catch (const std::exception &e) {
    std::cerr << "fccvis: " << e.what() << '\n';
    return 1;
  }
}
