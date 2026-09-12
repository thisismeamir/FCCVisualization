#pragma once

#include "Cluster.h"
#include "Track.h"
#include <array>
#include <memory>
#include <vector>

namespace fccvis::data::events {
class Vertex;
class Particle;
class Vertex {
public:
  int type = 0;

  double chi2 = 0.0;
  int ndf = 0;

  std::array<double, 3> position{};
  std::array<double, 6> covariance{};

  int algorithmType = 0;

  std::vector<double> parameters;

  std::vector<std::shared_ptr<fccvis::data::events::Particle>> particles;
};
class Particle {
public:
  int pdg = 0;

  double energy = 0.0;

  std::array<double, 3> momentum{};

  std::array<double, 3> referencePoint{};

  double charge = 0.0;
  double mass = 0.0;

  double goodnessOfPID = 0.0;

  std::array<double, 10> covariance{};

  std::shared_ptr<Vertex> decayVertex;

  std::vector<std::shared_ptr<fccvis::data::events::Cluster>> clusters;
  std::vector<std::shared_ptr<fccvis::data::events::Track>> tracks;
  std::vector<std::shared_ptr<Particle>> particles;
};
} // namespace fccvis::data::events
