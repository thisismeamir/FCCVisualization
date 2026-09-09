#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include "MetaObjects.h"
#include "SceneObjects.h"
#include "Scene.h"


namespace fccvis::session {

class SessionOptions
{

public:
  std::vector<std::shared_ptr<fccvis::scene::meta::Camera>> cameras;
  std::vector<std::shared_ptr<fccvis::scene::meta::Filter>> filters;
  std::vector<fccvis::scene::Scene> scenes;
  std::vector<std::vector<std::string>> sceneLayout;  // scene names shown together
  std::optional<std::filesystem::path> geometryFilePath;
  std::shared_ptr<scene::objects::DetectorGeometry> detectorGeometry;
};

struct Event
{
    std::vector<std::shared_ptr<scene::objects::Hit>> hits;
    std::vector<std::shared_ptr<scene::objects::Marker>> markers;
    std::vector<std::shared_ptr<scene::objects::Line>> lines;
    std::vector<std::shared_ptr<scene::objects::Track>> tracks;
};

class SessionData
{
public:
    std::vector<Event> events;
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
}
