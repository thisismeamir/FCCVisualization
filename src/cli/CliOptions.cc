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
    if (arg == "--gui")
      opts.guiRequested = true;
    else
      positional.emplace_back(arg);
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

CliOptions ParseArgs(int argc, char** argv)
{
    CliOptions opts;
    std::vector<std::filesystem::path> positional;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--gui")
            opts.guiRequested = true;
        else
            positional.emplace_back(arg);
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
  if (!opts.guiRequested && !opts.optionsFile)
    throw std::runtime_error(
      "Headless mode requires an options file specifying renderings (no --gui, no options.cpp given)");

  if (opts.dataFile && opts.dataFile->extension() == ".pndr")
    throw std::runtime_error(".pndr format is not yet supported (Pandora native format)");
} 
}

