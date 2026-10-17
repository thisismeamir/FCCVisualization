// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
#include "FileBackend.h"

#include <cctype>
#include <filesystem>
#include <stdexcept>

namespace fccvis::backend {

namespace {

std::string Sanitize(const std::string& name) {
  std::string out;
  for (char c : name)
    out += (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.') ? c : '_';
  return out.empty() ? std::string("_") : out;
}

}  // namespace

int FileBackend::IntOption(const Options& o, const char* key, int fallback) {
  auto it = o.find(key);
  if (it == o.end()) return fallback;
  try {
    std::size_t used = 0;
    const int v = std::stoi(it->second, &used);
    if (used != it->second.size() || v <= 0 || v > 16384) throw std::out_of_range(key);
    return v;
  } catch (const std::exception&) {
    throw std::invalid_argument(std::string("option '") + key + "' must be an integer in 1..16384, got '" +
                                it->second + "'");
  }
}

FileBackend::~FileBackend() { m_executor.reset(); }

void FileBackend::Start(const std::string& id, Port& port, const Options& options) {
  m_id = id;
  m_port = &port;
  m_width = IntOption(options, "width", m_width);
  m_height = IntOption(options, "height", m_height);
  if (auto it = options.find("output_dir"); it != options.end()) m_outputDir = it->second;
  if (auto it = options.find("tolerance"); it != options.end()) {
    try {
      m_tolerance = std::stod(it->second);
    } catch (const std::exception&) {
      throw std::invalid_argument("option 'tolerance' must be a number, got '" + it->second + "'");
    }
    if (!(m_tolerance > 0)) throw std::invalid_argument("option 'tolerance' must be positive");
  }
  if (auto it = options.find("async"); it != options.end()) m_async = it->second != "false" && it->second != "0";
  Configure(options);
  std::error_code ec;
  std::filesystem::create_directories(m_outputDir, ec);
  if (ec) throw std::runtime_error("cannot create output_dir '" + m_outputDir + "': " + ec.message());
  m_retained = std::make_unique<draw::RetainedScenes>(draw::ThemeDefaults(), m_tolerance, DrawnKinds());
  m_executor = std::make_unique<Executor>([this](const std::string& e) { Log(changes::Severity::Error, "render failed: " + e); });
  Log(changes::Severity::Info, Kind() + " canvas " + std::to_string(m_width) + "x" + std::to_string(m_height) +
                                   ", writing to '" + m_outputDir + "'");
}

void FileBackend::Stop() {
  if (m_executor) m_executor->Wait();
  m_executor.reset();
  Log(changes::Severity::Info, Kind() + " backend stopped");
  std::lock_guard<std::mutex> lock(m_mutex);
  m_port = nullptr;
}

bool FileBackend::OpenScene(const std::string& scene, const SessionView& view, std::string& error) {
  if (!view.FindScene(scene)) {
    error = "Scene '" + scene + "' does not exist.";
    return false;
  }
  return true;
}

void FileBackend::CloseScene(const std::string& scene) {
  m_tracker.Forget(scene);
  if (m_retained) m_retained->Forget(scene);
  std::lock_guard<std::mutex> lock(m_mutex);
  m_closed = true;
}

void FileBackend::Sync(const SyncRequest& request) {
  if (!m_retained) return;
  const SyncPlan plan = m_tracker.Update(request, m_id);
  m_retained->Apply(plan, request.view);
  auto job = std::make_shared<Job>();
  bool closed = false;
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    closed = m_closed;
    m_closed = false;
  }
  if (!plan.Empty() || closed) job->extra = Collect(plan, request);
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    Count(m_stats, plan);
    if (plan.Empty() && !closed) return;
    job->scenes = m_retained->Snapshot();
    job->layout = request.layout;
    job->open = request.open;
    job->generation = ++m_generation;
    m_pending = job;
  }
  auto render = [this] {
    std::shared_ptr<const Job> next;
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      if (!m_pending || m_pending->generation <= m_rendered) return;  // coalesced
      next = m_pending;
      m_rendered = next->generation;
    }
    Render(*next);
    std::lock_guard<std::mutex> lock(m_mutex);
    ++m_renders;
  };
  if (m_async && m_executor) m_executor->Post(render);
  else render();
}

void FileBackend::Flush() {
  if (m_executor) m_executor->Wait();
}

std::optional<DrawStats> FileBackend::Stats() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_stats;
}

std::uint64_t FileBackend::Renders() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_renders;
}

std::string FileBackend::ScenePath(const std::string& scene, const std::string& extension) const {
  return (std::filesystem::path(m_outputDir) / (Sanitize(scene) + extension)).string();
}

std::string FileBackend::OutputPath(const std::string& name) const {
  return (std::filesystem::path(m_outputDir) / name).string();
}

void FileBackend::Log(changes::Severity s, const std::string& m) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_port) m_port->Log(s, m);
}

}  // namespace fccvis::backend
