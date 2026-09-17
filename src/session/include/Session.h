#pragma once
#include "MetaObjects.h"
#include "Scene.h"
#include <filesystem>
#include <memory>
#include <optional>
#include <podio/Frame.h>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
namespace fccvis::session {

class SessionOptions {

public:
  std::vector<std::shared_ptr<fccvis::scene::meta::Camera>> cameras;
  std::vector<std::shared_ptr<fccvis::scene::meta::Filter>> filters;
  std::vector<std::shared_ptr<fccvis::scene::Scene>> scenes;
  std::shared_ptr<fccvis::scene::meta::LayoutNode>
      sceneLayout; // scene names shown together
  std::optional<std::filesystem::path> geometryFilePath;
};

class SessionData {
public:
  std::unordered_map<std::string, std::vector<podio::Frame>> categories;
};

class Session {
public:
  Session(std::string name, std::optional<std::filesystem::path> dataFilePath,
          std::optional<std::filesystem::path> optionsFilePath);
  ~Session();

  Session(const Session &) = delete;
  Session &operator=(const Session &) = delete;
  Session(Session &&) noexcept = default;
  Session &operator=(Session &&) noexcept = default;
  void initData();
  void initOptions();
  std::optional<std::filesystem::path> GetOptionsFile();
  SessionOptions GetOptions();
  const std::string &Name() const { return m_name; }
  const SessionData &Data() const { return m_data; }
  const SessionOptions &Options() const { return m_options; }
  std::vector<std::string> Categories() const;
  size_t EntryCount(const std::string &category) const;
  std::vector<std::string> CollectionNames(const std::string &category,
                                           size_t entryIndex) const;
  std::string CollectionType(const std::string &category, size_t entryIndex,
                             const std::string &collName) const;
  const podio::Frame &GetFrame(const std::string &category,
                               size_t entryIndex) const;

private:
  std::string m_name;
  std::optional<std::filesystem::path> m_dataFilePath;
  std::optional<std::filesystem::path> m_optionsFilePath;

  SessionData m_data;
  SessionOptions m_options;
};

} // namespace fccvis::session
