// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// B4 CED backend (T4.7): sends the scene to a CED display server (glced) over
// its TCP protocol (CedProtocol.h). CED shows one event: the backend draws the
// first pane of the layout (or the first open scene). Each fccvis layer is a
// CED visibility layer (described by name in glced's menu); points become
// CED hits, lines and surface outlines CED line segments. Double-clicking an
// object in glced returns its id, reported as a pick; glced closing the
// connection stops the backend.
#include "BackendRegistry.h"
#include "CedProtocol.h"
#include "FileBackend.h"

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <variant>

namespace fccvis::backend {

namespace {

using Entries = std::map<std::string, std::map<std::string, std::size_t>>;

std::uint32_t CedColor(const style::Color& c, float alpha) {
  const auto t = static_cast<std::uint32_t>(std::lround(255 * (1 - std::clamp(alpha, 0.f, 1.f))));
  return (t << 24) | (std::uint32_t(c.r) << 16) | (std::uint32_t(c.g) << 8) | c.b;
}

std::uint32_t Shape(style::MarkerShape m) {
  switch (m) {
    case style::MarkerShape::Square: return ced::kBox;
    case style::MarkerShape::Cross: return ced::kCross;
    case style::MarkerShape::Diamond:
    case style::MarkerShape::Triangle: return ced::kStar;
    default: return ced::kPoint;
  }
}

/** First pane of a layout (depth first), if any. */
std::optional<std::string> FirstPane(const scene::layout::LayoutNode& n) {
  return std::visit(
      [](const auto& v) -> std::optional<std::string> {
        using V = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<V, scene::layout::Pane>) {
          return v.sceneName;
        } else {
          for (const auto& c : v.children)
            if (auto s = FirstPane(c.node)) return s;
          return std::nullopt;
        }
      },
      n.value);
}

}  // namespace

/** CED backend (B4). */
class CedBackend final : public FileBackend {
 public:
  ~CedBackend() override { Disconnect(); }

  [[nodiscard]] Capabilities GetCapabilities() const override {
    Capabilities c;
    c.description = "CED event display (glced) over its TCP protocol: one scene, layers as CED visibility layers, picks";
    c.kinds = {ElementKind::Point, ElementKind::Line, ElementKind::Surface};
    c.styleProperties = {"color", "edgeColor", "edgeWidth", "marker", "opacity", "size", "visible", "width"};
    c.splits = false;  // CED shows one event: the first pane of the layout
    c.tabs = false;
    c.options = {{"host", "CED server host (default $CED_HOST or localhost)"},
                 {"port", "CED server port (default $CED_PORT or 7286, glced's default)"},
                 {"tolerance", "relative surface tessellation tolerance (default 0.02)"}};
    return c;
  }

  void Stop() override {
    FileBackend::Flush();
    Disconnect();
    FileBackend::Stop();
  }

 protected:
  [[nodiscard]] std::string Kind() const override { return "ced"; }

  void Configure(const Options& options) override {
    m_host = "localhost";
    if (const char* h = std::getenv("CED_HOST"); h && *h) m_host = h;
    m_cedPort = 7286;
    if (const char* p = std::getenv("CED_PORT"); p && *p) m_cedPort = std::atoi(p);
    if (auto it = options.find("host"); it != options.end()) m_host = it->second;
    if (auto it = options.find("port"); it != options.end()) {
      try {
        std::size_t used = 0;
        m_cedPort = std::stoi(it->second, &used);
        if (used != it->second.size()) throw std::out_of_range("port");
      } catch (const std::exception&) {
        m_cedPort = -1;
      }
    }
    if (m_cedPort < 1 || m_cedPort > 65535) throw std::invalid_argument("option 'port' must be an integer in 1..65535");
    Log(changes::Severity::Info, "sending to CED at " + m_host + ":" + std::to_string(m_cedPort));
  }

  [[nodiscard]] std::shared_ptr<const void> Collect(const SyncPlan&, const SyncRequest& request) override {
    auto out = std::make_shared<Entries>();
    for (const auto& name : request.open) {
      const auto scene = request.view.FindScene(name);
      if (!scene) continue;
      for (const auto& [layer, type] : scene->layers.Types()) {
        const auto kind = KindOf(type);
        if (!kind) continue;
        auto take = [&]<typename X>() {
          if (const auto* l = scene->layers.Find<X>(layer)) (*out)[name][layer] = l->entry.value_or(0);
        };
        switch (*kind) {
          case ElementKind::Point: take.template operator()<data::PointElement>(); break;
          case ElementKind::Line: take.template operator()<data::LineElement>(); break;
          case ElementKind::Surface: take.template operator()<data::SurfaceElement>(); break;
        }
      }
    }
    return out;
  }

  void Render(const Job& job) override {
    std::optional<std::string> shown;
    if (job.layout) shown = FirstPane(*job.layout);
    if (!shown && !job.open.empty()) shown = *job.open.begin();
    const auto entries = std::static_pointer_cast<const Entries>(job.extra);

    std::vector<ced::Hit> hits;
    std::vector<ced::Line> lines;
    std::vector<ced::LayerText> texts;
    std::vector<PickEvent> ids(1);  // id 0 = nothing
    auto newId = [&](const std::string& scene, const std::string& layer, std::size_t index) {
      PickEvent e;
      e.scene = scene;
      e.layer = layer;
      e.index = index;
      if (entries) {
        auto s = entries->find(scene);
        if (s != entries->end())
          if (auto l = s->second.find(layer); l != s->second.end()) e.entry = l->second;
      }
      ids.push_back(e);
      return static_cast<std::uint32_t>(ids.size() - 1);
    };
    if (shown) {
      auto it = job.scenes.find(*shown);
      if (it != job.scenes.end()) {
        std::uint32_t layerNo = 0;
        for (const auto& [name, drawing] : it->second.layers) {
          const std::uint32_t layer = std::min<std::uint32_t>(layerNo++, 99);
          ced::LayerText t;
          std::snprintf(t.text, sizeof t.text, "%s", name.c_str());
          t.layer = static_cast<std::int32_t>(layer);
          texts.push_back(t);
          if (!drawing) continue;
          for (const auto& p : drawing->points) {
            if (!draw::Visible(p.s.common)) continue;
            ced::Hit h{static_cast<float>(p.p[0]), static_cast<float>(p.p[1]), static_cast<float>(p.p[2])};
            h.shape = Shape(p.s.marker.value_or(style::MarkerShape::Circle));
            h.layer = layer;
            h.color = CedColor(draw::ColorOf(p.s.common), draw::Alpha(p.s.common));
            h.size = static_cast<std::uint32_t>(std::max(1.f, std::round(p.s.size.value_or(3.f))));
            h.id = newId(*shown, name, p.index);
            hits.push_back(h);
          }
          auto segment = [&](const draw::Vec3& a, const draw::Vec3& b, float width, std::uint32_t color, std::uint32_t id) {
            ced::Line l{static_cast<float>(a[0]), static_cast<float>(a[1]), static_cast<float>(a[2]),
                        static_cast<float>(b[0]), static_cast<float>(b[1]), static_cast<float>(b[2])};
            l.layer = layer;
            l.width = static_cast<std::uint32_t>(std::max(1.f, std::round(width)));
            l.color = color;
            l.id = id;
            lines.push_back(l);
          };
          for (const auto& l : drawing->lines) {
            if (!draw::Visible(l.s.common) || l.v.size() < 2) continue;
            const auto id = newId(*shown, name, l.index);
            const auto color = CedColor(draw::ColorOf(l.s.common), draw::Alpha(l.s.common));
            for (std::size_t k = 1; k < l.v.size(); ++k) segment(l.v[k - 1], l.v[k], l.s.width.value_or(1.f), color, id);
          }
          for (const auto& m : drawing->meshes) {
            if (!draw::Visible(m.s.common)) continue;
            const float w = m.s.edgeWidth.value_or(1.f);
            if (w <= 0) continue;
            const auto id = newId(*shown, name, m.index);
            const auto col = m.s.edgeColor.value_or(draw::ColorOf(m.s.common));
            const auto color = CedColor(col, col.a / 255.f);
            for (const auto& [a, b] : m.edges) segment(m.vertices[a], m.vertices[b], w, color, id);
          }
        }
      }
    }
    std::vector<char> buffer;
    ced::AppendMessage(buffer, ced::kLine, lines);
    ced::AppendMessage(buffer, ced::kHit, hits);
    ced::AppendMessage(buffer, ced::kLayerText, texts);
    ced::AppendDraw(buffer);
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_ids = std::move(ids);
    }
    Send(buffer);
  }

 private:
  void Send(const std::vector<char>& buffer) {
    if (m_fd < 0 && !Connect()) return;
    std::size_t sent = 0;
    while (sent < buffer.size()) {
      const ssize_t n = ::send(m_fd, buffer.data() + sent, buffer.size() - sent, MSG_NOSIGNAL);
      if (n <= 0) {
        Log(changes::Severity::Warning, "lost the connection to CED at " + m_host + ":" + std::to_string(m_cedPort) +
                                            " (" + std::strerror(errno) + "); will reconnect at the next update");
        Disconnect();
        return;
      }
      sent += static_cast<std::size_t>(n);
    }
  }

  bool Connect() {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    if (::getaddrinfo(m_host.c_str(), std::to_string(m_cedPort).c_str(), &hints, &res) != 0 || !res) {
      Warn("cannot resolve CED host '" + m_host + "'");
      return false;
    }
    const int fd = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    const bool ok = fd >= 0 && ::connect(fd, res->ai_addr, res->ai_addrlen) == 0;
    ::freeaddrinfo(res);
    if (!ok) {
      if (fd >= 0) ::close(fd);
      Warn("cannot reach CED at " + m_host + ":" + std::to_string(m_cedPort) + " (is glced running?); will retry");
      return false;
    }
    m_warned = false;
    m_fd = fd;
    m_reading = true;
    m_reader = std::thread([this, fd] { Read(fd); });
    Log(changes::Severity::Info, "connected to CED at " + m_host + ":" + std::to_string(m_cedPort));
    return true;
  }

  void Warn(const std::string& message) {
    if (m_warned) return;  // once per outage
    m_warned = true;
    Log(changes::Severity::Warning, message);
  }

  /** Reader thread: picks (one int id each) and the server closing the connection. */
  void Read(int fd) {
    std::int32_t id = 0;
    std::size_t have = 0;
    while (m_reading) {
      pollfd p{fd, POLLIN, 0};
      const int r = ::poll(&p, 1, 100);
      if (r <= 0) continue;
      const ssize_t n = ::recv(fd, reinterpret_cast<char*>(&id) + have, sizeof id - have, 0);
      if (n <= 0) {
        if (m_reading) {
          if (auto* port = GetPortForEvents()) port->Stopped("CED closed the connection");
        }
        return;
      }
      have += static_cast<std::size_t>(n);
      if (have < sizeof id) continue;
      have = 0;
      std::optional<PickEvent> e;
      {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (id > 0 && static_cast<std::size_t>(id) < m_ids.size()) e = m_ids[static_cast<std::size_t>(id)];
      }
      if (e)
        if (auto* port = GetPortForEvents()) port->Picked(*e);
    }
  }

  Port* GetPortForEvents() { return GetPort(); }

  void Disconnect() {
    m_reading = false;
    if (m_reader.joinable()) m_reader.join();
    if (m_fd >= 0) ::close(m_fd);
    m_fd = -1;
  }

  std::string m_host;
  int m_cedPort = 7286;
  int m_fd = -1;
  bool m_warned = false;
  std::atomic<bool> m_reading{false};
  std::thread m_reader;
  std::mutex m_mutex;          // guards m_ids
  std::vector<PickEvent> m_ids;  // id -> pick
};

static Registrar g_cedRegistrar("ced", [] { return std::make_unique<CedBackend>(); });

}  // namespace fccvis::backend
