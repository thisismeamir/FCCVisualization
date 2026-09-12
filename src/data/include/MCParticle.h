#pragma once

#include <array>
#include <memory>
#include <vector>

namespace fccvis::data::events {
class MCParticle {
public:
  int pdg = 0;

  int generatorStatus = 0;
  int simulatorStatus = 0;

  double charge = 0.0;
  double time = 0.0;
  double mass = 0.0;

  std::array<double, 3> vertex{};
  std::array<double, 3> endpoint{};

  std::array<double, 3> momentum{};
  std::array<double, 3> momentumAtEndpoint{};

  int helicity = 9;

  std::vector<std::shared_ptr<fccvis::data::events::MCParticle>> parents;
  std::vector<std::shared_ptr<MCParticle>> daughters;
};
} // namespace fccvis::data::events
