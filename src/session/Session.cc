#include "Session.h"

#include "Detector.h"

#include <DD4hep/Detector.h>
#include <podio/ROOTReader.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace fccvis::session {

Session::Session(std::string name,
                 std::optional<std::filesystem::path> dataFilePath,
                 std::optional<std::filesystem::path> optionsFilePath)
    : m_name(std::move(name)),
      m_dataFilePath(std::move(dataFilePath)),
      m_optionsFilePath(std::move(optionsFilePath)),
      m_data(),
      m_options(),
      m_detector() {
  if (m_dataFilePath) {
    initData();
  }

  if (m_optionsFilePath) {
    initOptions();
  }
}

Session::~Session() = default;

void Session::initData() {
  podio::ROOTReader reader;
  reader.openFile(m_dataFilePath->string());

  for (const auto &categoryView : reader.getAvailableCategories()) {
    const std::string category{categoryView};
    const auto nEntries = reader.getEntries(category);

    auto &frames = m_data.categories[category];
    frames.reserve(nEntries);

    for (unsigned i = 0; i < nEntries; ++i) {
      frames.emplace_back(reader.readNextEntry(category));
    }
  }
}

void Session::initOptions() {
  m_options = SessionOptions{};

  auto scene = std::make_shared<fccvis::scene::Scene>("default");
  m_options.scenes.emplace("default", std::move(scene));
}

std::optional<std::filesystem::path> Session::GetOptionsFile() const {
  return m_optionsFilePath;
}

SessionOptions Session::GetOptions() const {
  return m_options;
}

std::vector<std::string> Session::Categories() const {
  std::vector<std::string> names;
  names.reserve(m_data.categories.size());

  for (const auto &[name, frames] : m_data.categories) {
    names.push_back(name);
  }

  return names;
}

size_t Session::EntryCount(const std::string &category) const {
  const auto it = m_data.categories.find(category);

  return it == m_data.categories.end() ? 0 : it->second.size();
}

const podio::Frame &
Session::GetFrame(const std::string &category, size_t entryIndex) const {
  return m_data.categories.at(category).at(entryIndex);
}

std::vector<std::string>
Session::CollectionNames(const std::string &category,
                         size_t entryIndex) const {
  return GetFrame(category, entryIndex).getAvailableCollections();
}

std::string Session::CollectionType(const std::string &category,
                                    size_t entryIndex,
                                    const std::string &collName) const {
  const auto *collection = GetFrame(category, entryIndex).get(collName);

  return collection ? std::string{collection->getTypeName()} : std::string{};
}

void Session::InitializeDetectorGeometry(const std::string &mainXMLPath) {
  auto &detector = dd4hep::Detector::getInstance();

  detector.fromCompact(mainXMLPath);

  m_detector =
      fccvis::geometry::detector::BuildNode(detector.world());
}

} // namespace fccvis::session
