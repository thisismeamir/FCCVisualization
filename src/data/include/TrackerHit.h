#pragma once

#include <array>
#include <cstdint>
namespace fccvis::data::events {
class TrackerHit
{
public:

    uint64_t cellID = 0;
    int type = 0;
    int quality = 0;

    double time = 0.0;

    double energyDeposit = 0.0;
    double energyDepositError = 0.0;

    std::array<double, 3> position{};

    // 3x3 covariance
    std::array<double, 6> covariance{};
};
}
