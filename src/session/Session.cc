#include "Session.h"
#include "edm4hep/SimCalorimeterHitCollection.h"
#include "edm4hep/SimTrackerHitCollection.h"
#include "podio/Frame.h"
#include "podio/ROOTReader.h"
#include <algorithm>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace fccvis::session {

Session::Session(std::string name,
                 std::optional<std::filesystem::path> dataFilePath,
                 std::optional<std::filesystem::path> optionsFilePath)
    : m_name(std::move(name)), m_dataFilePath(std::move(dataFilePath)),
      m_optionsFilePath(std::move(optionsFilePath)), m_data(), m_options() {
  if (m_dataFilePath.has_value()) {
    Session::initData();
  }
  if (m_optionsFilePath.has_value()) {
    Session::initOptions();
  }
};

Session::~Session() = default;

void Session::initData() {
  podio::ROOTReader reader;
  reader.openFile(m_dataFilePath->string());

  const unsigned int numEvents = reader.getEntries("events");
  m_data.events.reserve(numEvents);

  for (unsigned int i = 0; i < numEvents; ++i) {
    auto frameData = reader.readEntry("events", i);
    podio::Frame frame(std::move(frameData));

    session::Event event;

    for (const auto &collectionName : frame.getAvailableCollections()) {
      const podio::CollectionBase *pCollection = frame.get(collectionName);
      if (pCollection == nullptr)
        continue;

      if (const auto *pTrackerHits =
              dynamic_cast<const edm4hep::SimCalorimeterHitCollection *>(
                  pCollection)) {
        for (const auto &hit : *pTrackerHits)
          event.markers.push_back(
              std::make_shared<scene::objects::Marker>(collectionName));
        // TODO: populate marker position from hit.getPosition()
      } else if (const auto *pCaloHits =
                     dynamic_cast<const edm4hep::SimCalorimeterHitCollection *>(
                         pCollection)) {
        for (const auto &hit : *pCaloHits)
          event.markers.push_back(
              std::make_shared<scene::objects::Marker>(collectionName));
        // TODO: populate marker position from hit.getPosition()
      }
      // TODO: other edm4hep collection types (tracks, clusters, MC particles,
      // ...)
    }

    m_data.events.push_back(std::move(event));
  }
};

void Session::initOptions() {
   m_options = SessionOptions{};
};
std::optional<std::filesystem::path> Session::GetOptionsFile() {
  return m_optionsFilePath;
}

SessionOptions Session::GetOptions() {
  return m_options;
}

} // namespace fccvis::session
