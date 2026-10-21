// Writes tests/data/edm4hep/events.edm4hep.root, the fixture of the EDM4hep
// adapter tests (3 events, every collection type the adapter maps plus a few
// it skips). Built as fccvis_edm4hep_make_fixture when FCCVIS_ENABLE_EDM4HEP
// is on; run it from tests/data/edm4hep to regenerate the committed file:
//   <build>/tests/fccvis_edm4hep_make_fixture events.edm4hep.root
// Values are deterministic functions of the event and object index.
#include <edm4hep/CalorimeterHitCollection.h>
#include <edm4hep/ClusterCollection.h>
#include <edm4hep/EventHeaderCollection.h>
#include <edm4hep/MCParticleCollection.h>
#include <edm4hep/RawCalorimeterHitCollection.h>
#include <edm4hep/ReconstructedParticleCollection.h>
#include <edm4hep/SimCalorimeterHitCollection.h>
#include <edm4hep/SimTrackerHitCollection.h>
#include <edm4hep/TrackCollection.h>
#include <edm4hep/TrackerHit3DCollection.h>
#include <podio/Frame.h>
#include <podio/UserDataCollection.h>
#include <podio/Writer.h>

#include <cmath>
#include <string>

int main(int argc, char** argv) {
  const std::string out = argc > 1 ? argv[1] : "events.edm4hep.root";
  auto writer = podio::makeWriter(out);
  for (int ev = 0; ev < 3; ++ev) {
    podio::Frame frame;

    edm4hep::EventHeaderCollection header;
    auto h = header.create();
    h.setEventNumber(100 + ev);
    h.setRunNumber(7);

    // Three MC particles: two with an endpoint, one without (endpoint = vertex).
    edm4hep::MCParticleCollection mc;
    for (int i = 0; i < 3; ++i) {
      auto p = mc.create();
      p.setPDG(i == 0 ? 11 : (i == 1 ? -13 : 22));
      p.setCharge(i == 0 ? -1.f : (i == 1 ? 1.f : 0.f));
      p.setGeneratorStatus(1);
      p.setSimulatorStatus(0);
      p.setMass(i == 0 ? 0.000511 : (i == 1 ? 0.10566 : 0.0));
      p.setVertex({0.0, 0.0, 1.0 * ev});
      const double phi = 0.5 * i + 0.1 * ev;
      p.setMomentum({3.0 * std::cos(phi), 3.0 * std::sin(phi), 1.0});
      if (i < 2) p.setEndpoint({1500.0 * std::cos(phi), 1500.0 * std::sin(phi), 500.0 + ev});
      else p.setEndpoint({0.0, 0.0, 1.0 * ev});
    }
    mc[1].addToParents(mc[0]);
    mc[0].addToDaughters(mc[1]);

    // Simulated tracker hits: 4 + ev per event, each linked to particle 0.
    edm4hep::SimTrackerHitCollection simHits;
    for (int i = 0; i < 4 + ev; ++i) {
      auto s = simHits.create();
      s.setCellID(1000 + i);
      s.setEDep(1e-5f * (i + 1));
      s.setTime(0.5f * i);
      s.setPathLength(0.3f);
      s.setPosition({100.0 * (i + 1), 10.0 * i, 5.0 * ev});
      s.setMomentum({1.f, 0.f, 0.f});
      s.setParticle(mc[0]);
    }

    edm4hep::TrackerHit3DCollection trkHits;
    for (int i = 0; i < 3; ++i) {
      auto t = trkHits.create();
      t.setCellID(2000 + i);
      t.setType(1);
      t.setEDep(2e-5f);
      t.setTime(1.f * i);
      t.setPosition({100.0 * (i + 1), 0.0, 10.0 * i});
    }

    edm4hep::CalorimeterHitCollection calo;
    for (int i = 0; i < 5; ++i) {
      auto c = calo.create();
      c.setCellID(3000 + i);
      c.setEnergy(0.5f * (i + 1) + ev);
      c.setTime(2.f);
      c.setType(i % 2);
      c.setPosition({1800.f, 20.f * i, -40.f + 20.f * i});
    }

    edm4hep::SimCalorimeterHitCollection simCalo;
    for (int i = 0; i < 2; ++i) {
      auto c = simCalo.create();
      c.setCellID(4000 + i);
      c.setEnergy(0.25f * (i + 1));
      c.setPosition({1800.f, -20.f * i, 0.f});
    }

    edm4hep::RawCalorimeterHitCollection raw;  // no position: skipped by the adapter
    for (int i = 0; i < 2; ++i) {
      auto r = raw.create();
      r.setCellID(5000 + i);
      r.setAmplitude(10 * i);
    }

    // One track with states at the IP, first hit, last hit (out of order on purpose).
    edm4hep::TrackCollection tracks;
    auto trk = tracks.create();
    trk.setType(3);
    trk.setChi2(1.5f);
    trk.setNdf(4);
    for (int i = 0; i < 3; ++i) trk.addToTrackerHits(trkHits[i]);
    edm4hep::TrackState last;
    last.location = edm4hep::TrackState::AtLastHit;
    last.referencePoint = {300.f, 0.f, 20.f};
    last.D0 = 0.f;
    last.Z0 = 0.f;
    edm4hep::TrackState ip;
    ip.location = edm4hep::TrackState::AtIP;
    ip.referencePoint = {0.f, 0.f, 0.f};
    ip.D0 = 1.f;    // point on the track: (-D0 sin phi, D0 cos phi, Z0) = (0, 1, 2) for phi = 0
    ip.Z0 = 2.f;
    ip.phi = 0.f;
    ip.omega = 1e-4f;
    ip.tanLambda = 0.1f;
    edm4hep::TrackState first;
    first.location = edm4hep::TrackState::AtFirstHit;
    first.referencePoint = {100.f, 0.f, 0.f};
    trk.addToTrackStates(last);
    trk.addToTrackStates(ip);
    trk.addToTrackStates(first);

    edm4hep::ClusterCollection clusters;
    auto cl = clusters.create();
    cl.setEnergy(3.f + ev);
    cl.setType(2);
    cl.setPosition({1800.f, 40.f, 0.f});
    cl.addToHits(calo[0]);
    cl.addToHits(calo[1]);

    edm4hep::ReconstructedParticleCollection reco;
    for (int i = 0; i < 2; ++i) {
      auto r = reco.create();
      r.setPDG(i == 0 ? 11 : 22);
      r.setEnergy(5.f + i);
      r.setCharge(i == 0 ? -1.f : 0.f);
      r.setMass(0.f);
      r.setReferencePoint({0.f, 0.f, 0.f});
      r.setMomentum({0.f, 4.f + i, 3.f});
      if (i == 0) r.addToTracks(trk);
      r.addToClusters(cl);
    }

    podio::UserDataCollection<float> weights;  // not an EDM4hep type: skipped
    weights.push_back(0.5f);

    frame.put(std::move(header), "EventHeader");
    frame.put(std::move(mc), "MCParticles");
    frame.put(std::move(simHits), "VertexBarrelHits");
    frame.put(std::move(trkHits), "TrackerHits");
    frame.put(std::move(calo), "ECalBarrelHits");
    frame.put(std::move(simCalo), "ECalBarrelSimHits");
    frame.put(std::move(raw), "ECalRawHits");
    frame.put(std::move(tracks), "Tracks");
    frame.put(std::move(clusters), "Clusters");
    frame.put(std::move(reco), "PandoraPFOs");
    frame.put(std::move(weights), "Weights");
    writer.writeFrame(frame, "events");
  }
  writer.finish();
  return 0;
}
