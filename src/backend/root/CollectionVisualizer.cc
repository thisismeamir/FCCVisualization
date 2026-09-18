#include "CollectionVisualizer.h"

#include <edm4hep/CalorimeterHitCollection.h>
#include <edm4hep/ClusterCollection.h>
#include <edm4hep/SimTrackerHitCollection.h>
#include <edm4hep/TrackCollection.h>
#include <edm4hep/TrackerHit3DCollection.h>

#include <TEveBoxSet.h>
#include <TEveElement.h>
#include <TEvePointSet.h>
#include <TEveTrack.h>
#include <TEveTrackPropagator.h>

#include <iostream>
#include <podio/CollectionBase.h>

namespace fccvis::backend::root {

TEveElement* CollectionVisualizer::VisualizeFrame(const podio::Frame& frame) {
  auto* frameList = new TEveElementList("EventFrame");

  const auto& collectionNames = frame.getAvailableCollections();
  for (const auto& name : collectionNames) {
    if (TEveElement* elem = VisualizeCollection(frame, name)) {
      frameList->AddElement(elem);
    }
  }

  return frameList;
}

TEveElement* CollectionVisualizer::VisualizeCollection(const podio::Frame& frame, std::string_view collectionName) {
  const std::string nameStr(collectionName);

  const podio::CollectionBase* coll = frame.get(nameStr);
  if (!coll || !coll->hasID()) return nullptr;
  const std::string typeName = std::string{coll->getValueTypeName()};

  if (typeName == "edm4hep::CalorimeterHitCollection") {
    return ConvertCalorimeterHits(frame, collectionName);
  } else if (typeName == "edm4hep::TrackerHit3DCollection") {
    return ConvertTrackerHits(frame, collectionName);
  } else if (typeName == "edm4hep::SimTrackerHitCollection") {
    return ConvertSimTrackerHits(frame, collectionName);
  } else if (typeName == "edm4hep::TrackCollection") {
    return ConvertTracks(frame, collectionName);
  } else if (typeName == "edm4hep::ClusterCollection") {
    return ConvertClusters(frame, collectionName);
  }

  return nullptr;
}

TEveElement* CollectionVisualizer::ConvertCalorimeterHits(const podio::Frame& frame, std::string_view collectionName) {
  const std::string nameStr(collectionName);
  const auto& hits = frame.get<edm4hep::CalorimeterHitCollection>(nameStr);
  if (!hits.hasID()) return nullptr;

  auto* boxSet = new TEveBoxSet(nameStr.c_str());
  boxSet->Reset(TEveBoxSet::kBT_AABox, kFALSE, hits.size());

  for (const auto& hit : hits) {
    const auto& pos = hit.getPosition();
    // Default box dimensions (10mm x 10mm x 10mm), styled/scaled downstream
    boxSet->AddBox(
      static_cast<Float_t>(pos.x - 5.0),
      static_cast<Float_t>(pos.y - 5.0),
      static_cast<Float_t>(pos.z - 5.0),
      10.0f, 10.0f, 10.0f
    );
    boxSet->DigitValue(static_cast<Int_t>(hit.getEnergy() * 1000.0)); // Store scaled energy into digit value
  }

  boxSet->RefitPlex();
  return boxSet;
}

TEveElement* CollectionVisualizer::ConvertTrackerHits(const podio::Frame& frame, std::string_view collectionName) {
  const std::string nameStr(collectionName);
  const auto& hits = frame.get<edm4hep::TrackerHit3DCollection>(nameStr);
  if (!hits.hasID()) return nullptr;

  auto* pointSet = new TEvePointSet(nameStr.c_str());
  pointSet->Reset(hits.size());

  Int_t pointIdx = 0;
  for (const auto& hit : hits) {
    const auto& pos = hit.getPosition();
    pointSet->SetPoint(pointIdx++, pos.x, pos.y, pos.z);
  }

  return pointSet;
}

TEveElement* CollectionVisualizer::ConvertSimTrackerHits(const podio::Frame& frame, std::string_view collectionName) {
  const std::string nameStr(collectionName);
  const auto& hits = frame.get<edm4hep::SimTrackerHitCollection>(nameStr);
  if (!hits.hasID()) return nullptr;

  auto* pointSet = new TEvePointSet(nameStr.c_str());
  pointSet->Reset(hits.size());

  Int_t pointIdx = 0;
  for (const auto& hit : hits) {
    const auto& pos = hit.getPosition();
    pointSet->SetPoint(pointIdx++, pos.x, pos.y, pos.z);
  }

  return pointSet;
}

TEveElement* CollectionVisualizer::ConvertTracks(const podio::Frame& frame, std::string_view collectionName) {
  const std::string nameStr(collectionName);
  const auto& tracks = frame.get<edm4hep::TrackCollection>(nameStr);
  if (!tracks.hasID()) return nullptr;

  auto* trackList = new TEveTrackList(nameStr.c_str());
  auto* propagator = trackList->GetPropagator();
  propagator->SetMagField(2.0); // Default B-field estimate; overridden by RootScene configuration if required

  Int_t trackIdx = 0;
  for (const auto& track : tracks) {
    if (track.trackStates_size() == 0) continue;

    const auto& state = track.getTrackStates(0); // Take reference track state at IP/first layer
    const auto& pos = state.referencePoint;

    // Convert helical parameters to momentum estimates (pX, pY, pZ)
    const double omega = state.omega;
    const double pt = (std::abs(omega) > 1e-6) ? (0.3 * 2.0 / (std::abs(omega) * 1000.0)) : 1.0;
    const double px = pt * std::cos(state.phi);
    const double py = pt * std::sin(state.phi);
    const double pz = pt * state.tanLambda;

    TEveRecTrackD recTrack;
    recTrack.fV.Set(pos.x, pos.y, pos.z);
    recTrack.fP.Set(px, py, pz);
    recTrack.fSign = (omega >= 0) ? 1 : -1;

    auto* eveTrack = new TEveTrack(&recTrack, propagator);
    eveTrack->SetName(Form("Track_%d", trackIdx++));
    eveTrack->MakeTrack();
    trackList->AddElement(eveTrack);
  }

  return trackList;
}

TEveElement* CollectionVisualizer::ConvertClusters(const podio::Frame& frame, std::string_view collectionName) {
  const std::string nameStr(collectionName);
  const auto& clusters = frame.get<edm4hep::ClusterCollection>(nameStr);
  if (!clusters.hasID()) return nullptr;

  auto* clusterList = new TEveElementList(nameStr.c_str());

  Int_t clusterIdx = 0;
  for (const auto& cluster : clusters) {
    auto* hitPoints = new TEvePointSet(Form("Cluster_%d", clusterIdx++));
    hitPoints->Reset(cluster.hits_size());

    Int_t pointIdx = 0;
    for (const auto& hit : cluster.getHits()) {
      const auto& pos = hit.getPosition();
      hitPoints->SetPoint(pointIdx++, pos.x, pos.y, pos.z);
    }
    clusterList->AddElement(hitPoints);
  }

  return clusterList;
}

} // namespace fccvis::backend::root
