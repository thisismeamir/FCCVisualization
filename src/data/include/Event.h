#pragma once

#include "CaloriMeterHit.h"
#include "Cluster.h"
#include "DetectorGeometry.h"
#include "MCParticle.h"
#include "Particle.h"
#include "ParticleFlowObject.h"
#include "Track.h"
#include "TrackState.h"
#include "TrackerHit.h"
#include <vector>
namespace fccvis::data::event {

struct Event {

  std::vector<fccvis::data::events::CalorimeterHit> calhits;
  std::vector<fccvis::data::events::TrackerHit> trahits;
  std::vector<fccvis::data::events::Cluster> clusters;
  std::vector<fccvis::data::events::DetectorGeometry> detectorGeometries;
  std::vector<fccvis::data::events::Particle> particles;
  std::vector<fccvis::data::events::ParticleFlowObject> particleFlowObjects;
  std::vector<fccvis::data::events::MCParticle> mcParticles;
  std::vector<fccvis::data::events::Track> tracks;
  std::vector<fccvis::data::events::TrackState> trackstate;
};

} // namespace fccvis::data::event
