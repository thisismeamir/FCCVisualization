#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace fccvis::data::events {

class DetectorGeometry {
public:
    DetectorGeometry(std::string name)
        : name(std::move(name)) {}

    std::string name;

    std::optional<std::filesystem::path> sourcePath;
};

}
