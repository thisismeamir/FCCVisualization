#include "Session.h"

#include "podio/Frame.h"
#include "podio/ROOTReader.h"
#include <filesystem>
#include <memory>
#include <optional>
#include <podio/CollectionBase.h>
#include <string>
#include <string_view>
#include <unordered_map>
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

void Session::initOptions() { m_options = SessionOptions{};
  const bool hasDefault = std::any_of(
      m_options.scenes.begin(), m_options.scenes.end(),
      [](const auto &s) { return s->Name() == "default"; });

  if (!hasDefault) {
    auto scene = std::make_shared<fccvis::scene::Scene>("default", "default");
    scene->camera = std::make_shared<fccvis::scene::meta::Camera>("default");
    m_options.scenes.push_back(std::move(scene));
  }
};
std::optional<std::filesystem::path> Session::GetOptionsFile() {
  return m_optionsFilePath;
}

SessionOptions Session::GetOptions() { return m_options; }

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

const podio::Frame &Session::GetFrame(const std::string &category,
                                      size_t entryIndex) const {
  return m_data.categories.at(category).at(entryIndex);
}

std::vector<std::string> Session::CollectionNames(const std::string &category,
                                                  size_t entryIndex) const {
  return GetFrame(category, entryIndex).getAvailableCollections();
}

std::string Session::CollectionType(const std::string &category,
                                    size_t entryIndex,
                                    const std::string &collName) const {
  const auto *coll = GetFrame(category, entryIndex).get(collName);
  return coll ? std::string{coll->getTypeName()} : std::string{};
}
} // namespace fccvis::session
