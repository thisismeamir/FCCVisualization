#pragma once
#include "BaseSessionObject.h"
#include <utility>
#include <string>

namespace fccvis::scene::objects {

class Hit : public BaseSessionObject {
  public:
  Hit(std::string hitName)
  : BaseSessionObject(std::move(hitName)) {}
};
class Marker : public BaseSessionObject {
  public:
  Marker(std::string markerName)
  : BaseSessionObject(std::move(markerName)) {}
};
class Line : public BaseSessionObject {
  public:
  Line(std::string lineName)
  : BaseSessionObject(std::move(lineName)) {}
};
class Track : public BaseSessionObject {
  public:
  Track(std::string trackName)
  : BaseSessionObject(std::move(trackName)) {}

};
class DetectorGeometry : public BaseSessionObject {
  public:
  DetectorGeometry(std::string detectorGeometryName)
  : BaseSessionObject(std::move(detectorGeometryName)) {}
};



}
