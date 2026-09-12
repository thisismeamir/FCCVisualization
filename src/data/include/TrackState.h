#pragma once

#include <array>
namespace fccvis::data::events {
class TrackState {
public:
  int location = 0;

  double d0 = 0.0;
  double phi = 0.0;
  double omega = 0.0;
  double z0 = 0.0;
  double tanLambda = 0.0;

  double time = 0.0;

  std::array<double, 3> referencePoint{};

  std::array<double, 21> covariance{};
};
} // namespace fccvis::data::events
