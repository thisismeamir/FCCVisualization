#pragma once
#include <filesystem>
#include <optional>
namespace fccvis::cli {
struct CliOptions {
  bool guiRequested = false;
  bool interactive = true;
  std::optional<std::filesystem::path> dataFile;
  std::optional<std::filesystem::path> optionsFile;
};

CliOptions ParseArgs(int argc, char** argv);
void ValidateArgs(const CliOptions& opts);
} // namespace fccvis::cli
