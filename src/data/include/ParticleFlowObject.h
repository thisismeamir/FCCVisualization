#pragma once

#include "Track.h"
#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace fccvis::data::events {

class Track;
class Cluster;
class Vertex;
class ParticleFlowObject;

class ParticleFlowObject {
public:
    ParticleFlowObject(std::string name)
        : name(std::move(name)) {}

    std::string name;

    int particleId = 0;

    int charge = 0;

    double mass = 0.0;

    double energy = 0.0;

    std::array<double, 3> momentum{};

    std::vector<std::shared_ptr<Track>> tracks;

    std::vector<std::shared_ptr<Cluster>> clusters;

    std::vector<std::shared_ptr<Vertex>> vertices;

    std::vector<std::shared_ptr<ParticleFlowObject>> parents;

    std::vector<std::shared_ptr<ParticleFlowObject>> daughters;

    std::unordered_map<std::string, double> properties;
};

}
