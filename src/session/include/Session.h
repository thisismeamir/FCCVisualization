#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "MetaObjects.h"
#include "Scene.h"
#include "Event.h"
#include "DetectorGeometry.h"

namespace fccvis::session {

class SessionOptions
{

public:
  std::vector<std::shared_ptr<fccvis::scene::meta::Camera>> cameras;
  std::vector<std::shared_ptr<fccvis::scene::meta::Filter>> filters;
  std::vector<std::shared_ptr<fccvis::scene::Scene>> scenes;
  std::shared_ptr<fccvis::scene::meta::LayoutNode> sceneLayout;  // scene names shown together
  std::optional<std::filesystem::path> geometryFilePath;
  std::shared_ptr<fccvis::data::events::DetectorGeometry> detectorGeometry;
};


class SessionData
{
public:
    std::vector<fccvis::data::event::Event> events;

    SessionData(std::filesystem::path& dataFilePath);
};


class Session
{
public:
    Session(std::string name,
            std::optional<std::filesystem::path> dataFilePath,
            std::optional<std::filesystem::path> optionsFilePath);
    ~Session();

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    Session(Session&&) noexcept = default;
    Session& operator=(Session&&) noexcept = default;
    void initData();
    void initOptions();
    std::optional<std::filesystem::path> GetOptionsFile();
    SessionOptions GetOptions();
  const std::string& Name() const { return m_name; }
    const SessionData& Data() const { return m_data; }
    const SessionOptions& Options() const { return m_options; }

private:
    std::string m_name;
    std::optional<std::filesystem::path> m_dataFilePath;
    std::optional<std::filesystem::path> m_optionsFilePath;

    SessionData m_data;
    SessionOptions m_options;
};


Session* makeDefaultSession() {
  Session *defaultSession = new Session(
    "Default Session",
    "",
    ""
  );


  return defaultSession;
};
}
