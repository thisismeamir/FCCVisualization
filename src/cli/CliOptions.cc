#include "include/CliOptions.h"
#include <filesystem>
#include <vector>
namespace fccvis::cli {

CliOptions ParseArgs(int argc, char** argv)
{
  CliOptions opts;
  std::vector<std::filesystem::path> positional;

  for (int i = 1; i < argc; ++i)
  {
    const std::string arg = argv[i];
    if (arg == "--gui") {
      opts.guiRequested = true;
    }
    if (arg == "--nointeractive") {
      opts.interactive = true;
    }
    else {
      positional.emplace_back(arg);
    }
  }

  for (const auto& p : positional)
  {
    const auto ext = p.extension();
    if (ext == ".cpp")
      opts.optionsFile = p;
    else if (ext == ".root" || ext == ".pndr")
      opts.dataFile = p;
    else
      throw std::runtime_error("Unrecognised argument: " + p.string());
  }

  return opts;
}


void ValidateArgs(const CliOptions& opts)
{
  // We had a --gui / <optionsFile> comparidon here that if gui wasn't flagged and optionsFile weren't
  // given as well what should we do with the data, not a blank interactive session is valid therefore I'm not setting that
  if (opts.dataFile && opts.dataFile->extension() == ".pndr")
    throw std::runtime_error(".pndr format is not yet supported (Pandora native format)");
  
}
}
