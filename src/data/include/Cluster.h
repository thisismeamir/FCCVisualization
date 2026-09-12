#pragma once

#include "CaloriMeterHit.h"
#include <array>
#include <memory>
#include <vector>
namespace fccvis::data::events {
class Cluster {
public:
    int type = 0;

    double energy = 0.0;
    double energyError = 0.0;

    std::array<double, 3> position{};
    std::array<double, 6> positionCovariance{};

    double iTheta = 0.0;
    double iPhi = 0.0;

    std::array<double, 6> directionError{};

    std::vector<double> shapeParameters;
    std::vector<double> subdetectorEnergies;

    std::vector<std::shared_ptr<fccvis::data::events::CalorimeterHit>> hits;
    std::vector<std::shared_ptr<Cluster>> subClusters;
};
}
