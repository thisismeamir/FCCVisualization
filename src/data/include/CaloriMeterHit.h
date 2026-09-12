#pragma once


#include <array>
#include <cstdint>
namespace fccvis::data::events {
class CalorimeterHit
{
public:
    uint64_t cellID = 0;
    double energy = 0.0;
    double energyError = 0.0;
    double time = 0.0;

    std::array<double, 3> position{};

    int type = 0;
};

}
