// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
#include "fccvis/api/Interpreter.h"

#include "AdapterRegistry.h"
#include "BackendRegistry.h"
#include "CameraChanges.h"
#include "Drawing.h"
#include "SceneChanges.h"
#include "fccvis/api/Config.h"
#include "fccvis/api/Expression.h"
#include "fccvis/api/Script.h"
#include "fccvis/stdlib/Styles.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>
#include <set>
#include <sstream>
#include <typeindex>

namespace fccvis::api {

namespace {

using PE = data::PointElement;
using LE = data::LineElement;
using SE = data::SurfaceElement;
using data::ElementKind;

struct Word {
  std::string text;
  std::size_t begin = 0;  // offset in the line
};

std::vector<Word> Words(std::string_view line) {
  std::vector<Word> out;
  std::size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
    if (i >= line.size()) break;
    Word w;
    w.begin = i;
    while (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i]))) {
      if (line[i] == '"') {
        const std::size_t col = i + 1;
        ++i;
        bool closed = false;
        while (i < line.size()) {
          if (line[i] == '\\' && i + 1 < line.size()) {
            w.text += line[i + 1];
            i += 2;
          } else if (line[i] == '"') {
            ++i;
            closed = true;
            break;
          } else {
            w.text += line[i++];
          }
        }
        if (!closed) throw ParseError("unterminated string", col);
      } else {
        w.text += line[i++];
      }
    }
    out.push_back(std::move(w));
  }
  return out;
}

std::string Trim(std::string_view s) {
  std::size_t a = 0, b = s.size();
  while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
  return std::string(s.substr(a, b - a));
}

std::string Num(double v) { return backend::draw::FormatNumber(v, 9); }

std::string KindName(ElementKind k) { return data::ToString(k); }

std::optional<double> ToNumber(const std::string& s) {
  double v = 0;
  const char* b = s.data();
  const char* e = b + s.size();
  if (b != e && *b == '+') ++b;
  auto [p, ec] = std::from_chars(b, e, v);
  if (ec != std::errc() || p != e || b == e || !std::isfinite(v)) return std::nullopt;
  return v;
}

std::string Join(const std::vector<std::string>& v, const std::string& sep) {
  std::string out;
  for (std::size_t i = 0; i < v.size(); ++i) out += (i ? sep : "") + v[i];
  return out;
}

/** Calls f.template operator()<X>() for the element type of kind k. */
template <typename F>
decltype(auto) WithKind(ElementKind k, F&& f) {
  switch (k) {
    case ElementKind::Point: return f.template operator()<PE>();
    case ElementKind::Line: return f.template operator()<LE>();
    case ElementKind::Surface: break;
  }
  return f.template operator()<SE>();
}

class CommandError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

bool Glob(const std::string& pattern, const std::string& s) {
  // '*' and '?' wildcards
  std::size_t p = 0, i = 0, star = std::string::npos, mark = 0;
  while (i < s.size()) {
    if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == s[i])) {
      ++p;
      ++i;
    } else if (p < pattern.size() && pattern[p] == '*') {
      star = p++;
      mark = i;
    } else if (star != std::string::npos) {
      p = star + 1;
      i = ++mark;
    } else {
      return false;
    }
  }
  while (p < pattern.size() && pattern[p] == '*') ++p;
  return p == pattern.size();
}

}  // namespace

// ---------------------------------------------------------------------------
// Shared state with the session log sink (may be called from backend threads)
// ---------------------------------------------------------------------------
struct Interpreter::Shared {
  std::mutex mutex;
  std::deque<std::pair<changes::Severity, std::string>> log;  // last messages
  std::function<void(changes::Severity, const std::string&)> sink;
  changes::Severity minimum = changes::Severity::Warning;
  // Messages emitted by the command being executed (on its thread) go into
  // its reply instead of the sink: warnings as output lines, errors dropped
  // (the reply reports them already).
  std::thread::id executing{};
  std::vector<std::string> pendingWarnings;
  bool captureUrls = false;              // `backend init`: show where a backend serves its output
  std::vector<std::string> pendingUrls;

  void Add(changes::Severity s, const std::string& m) {
    std::function<void(changes::Severity, const std::string&)> forward;
    {
      std::lock_guard<std::mutex> lock(mutex);
      log.emplace_back(s, m);
      while (log.size() > 1000) log.pop_front();
      if (executing == std::this_thread::get_id()) {
        if (s == changes::Severity::Warning) pendingWarnings.push_back(m);
        if (s == changes::Severity::Info && captureUrls && m.find("http://") != std::string::npos) pendingUrls.push_back(m);
        return;
      }
      if (sink && static_cast<int>(s) >= static_cast<int>(minimum)) forward = sink;
    }
    if (forward) forward(s, m);
  }
};

struct Interpreter::Line {
  std::string raw;
  std::vector<Word> words;
  /** Raw text from word @p k to the end of the line. */
  [[nodiscard]] std::string Rest(std::size_t k) const { return k < words.size() ? Trim(raw.substr(words[k].begin)) : std::string(); }
  [[nodiscard]] std::size_t Offset(std::size_t k) const {
    if (k >= words.size()) return raw.size();
    std::size_t o = words[k].begin;
    while (o < raw.size() && std::isspace(static_cast<unsigned char>(raw[o]))) ++o;
    return o;
  }
  [[nodiscard]] const std::string& W(std::size_t k) const {
    static const std::string empty;
    return k < words.size() ? words[k].text : empty;
  }
  [[nodiscard]] std::size_t Size() const { return words.size(); }
};

std::string Reply::Text() const {
  std::string s;
  for (const auto& l : lines) s += l + "\n";
  for (const auto& e : errors) s += "error: " + e + "\n";
  return s;
}

Interpreter::Interpreter(session::Session& session) : m_session(session), m_shared(std::make_shared<Shared>()) {
  std::weak_ptr<Shared> weak = m_shared;
  m_session.SetLogSink([weak](changes::Severity s, const std::string& m) {
    if (auto sh = weak.lock()) sh->Add(s, m);
  });
}

Interpreter::~Interpreter() { m_session.SetLogSink(nullptr); }

void Interpreter::SetLogSink(std::function<void(changes::Severity, const std::string&)> sink,
                             changes::Severity minimum) {
  std::lock_guard<std::mutex> lock(m_shared->mutex);
  m_shared->sink = std::move(sink);
  m_shared->minimum = minimum;
}

std::string Interpreter::Resolve(const std::string& path) const {
  if (m_base.empty() || path.empty() || std::filesystem::path(path).is_absolute()) return path;
  return (std::filesystem::path(m_base) / path).lexically_normal().string();
}

// ---------------------------------------------------------------------------
// Command forms (help and completion)
// ---------------------------------------------------------------------------
const std::vector<CommandForm>& Interpreter::Forms() {
  using A = ArgKind;
  static const std::vector<CommandForm> forms = {
      {{"load"}, {A::Adapter, A::File, A::AdapterOption}, {}, "load <adapter> <path> [key=value ...]",
       "open a source with an adapter and register its series"},
      {{"series"}, {A::Text}, {}, "series [pattern]", "list registered series (pattern with * and ?)"},
      {{"describe"}, {A::Series}, {}, "describe <series>", "show a series: kind, entries, metadata keys of entry 0"},
      {{"scene", "new"}, {A::Name}, {}, "scene new <name> ...", "create scenes, each with its own camera"},
      {{"scene", "rm"}, {A::Scene}, {}, "scene rm <name> ...", "remove scenes"},
      {{"scene", "ls"}, {}, {}, "scene ls", "list scenes"},
      {{"scene", "show"}, {A::Scene}, {}, "scene show <name>", "show a scene: layers, filters, styles, camera"},
      {{"layer", "add"}, {A::Scene, A::Series, A::Keyword, A::Name}, {"as", "entry"},
       "layer add <scene> <series> [as <layer>] [entry <n>]", "show a series in a scene (layer name defaults to the series)"},
      {{"layer", "rm"}, {A::LayerPath}, {}, "layer rm <scene>/<layer>", "remove a layer"},
      {{"layer", "ls"}, {A::Scene}, {}, "layer ls <scene>", "list the layers of a scene"},
      {{"where"}, {A::LayerPath, A::Filter}, {}, "where <scene>/<layer> <filter-expression>",
       "select the elements a layer shows (see help filters)"},
      {{"style"}, {A::LayerPath, A::Style}, {}, "style <scene>/<layer> [when <filter> then] key=value ...",
       "add a style rule (see help styles)"},
      {{"style"}, {A::LayerPath, A::Keyword, A::Text}, {"preset"}, "style <scene>/<layer> preset <name> [args]",
       "add a stdlib style preset (see help styles)"},
      {{"env"}, {A::Scene, A::Environment}, {}, "env <scene> key=value ... | env <scene> preset <name>",
       "set the scene environment: background, axes, grid"},
      {{"camera"}, {A::Scene, A::Keyword, A::Number, A::Number, A::Number},
       {"dir", "pos", "zoom", "reset", "mode", "share"},
       "camera <scene> dir|pos <x> <y> <z> | zoom <m> | reset | mode free|fixed|animated | share <scene>",
       "change a scene's camera"},
      {{"layout"}, {A::Layout}, {}, "layout <expression> | layout show",
       "set the layout, e.g. split(xy as \"Front\", tabs(rz, xz)) (see help layouts)"},
      {{"entry"}, {A::LayerPath, A::Keyword}, {"next", "prev", "first", "last"},
       "entry <scene>/<layer> <n>|next|prev|first|last", "move a layer to another entry (event)"},
      {{"backend", "init"}, {A::BackendType, A::Name, A::BackendOption}, {},
       "backend init <type> [id] [key=value ...]", "attach a backend instance"},
      {{"backend", "rm"}, {A::BackendId}, {}, "backend rm <id>", "detach a backend"},
      {{"backend", "ls"}, {}, {}, "backend ls", "list attached backends and their open scenes"},
      {{"backend", "flush"}, {A::BackendId}, {}, "backend flush [id]", "wait until backends finished their work"},
      {{"open"}, {A::BackendId, A::Scene}, {}, "open <backend> <scene> ...", "show scenes in a backend"},
      {{"close"}, {A::BackendId, A::Scene}, {}, "close <backend> <scene> ...", "stop showing scenes in a backend"},
      {{"log"}, {A::Number}, {}, "log [n]", "show the last n session messages (default 20)"},
      {{"save"}, {A::File}, {}, "save <file>", "write the session configuration (JSON)"},
      {{"source"}, {A::File}, {}, "source <file>", "run a script or load a saved configuration"},
      {{"echo"}, {A::Text}, {}, "echo <text>", "print text"},
      {{"undo"}, {}, {}, "undo", "undo the last command that changed scenes, layers, cameras or the layout"},
      {{"redo"}, {}, {}, "redo", "redo the last undone command"},
      {{"help"}, {A::Topic, A::Name}, {}, "help [command | adapter <name> | backend <type> | filters | styles | layouts]",
       "show help"},
      {{"quit"}, {}, {}, "quit", "leave the shell (also: exit)"},
  };
  return forms;
}

// ---------------------------------------------------------------------------
// Execution
// ---------------------------------------------------------------------------
Reply Interpreter::Execute(std::string_view text) {
  Reply r;
  Line line;
  line.raw = std::string(text);
  if (!line.raw.empty() && line.raw.back() == '\r') line.raw.pop_back();
  const std::string trimmed = Trim(line.raw);
  if (trimmed.empty() || trimmed[0] == '#') return r;
  struct Executing {
    Shared& sh;
    Reply* reply = nullptr;
    std::thread::id previous;
    explicit Executing(Shared& s) : sh(s) {
      std::lock_guard<std::mutex> lock(sh.mutex);
      previous = sh.executing;
      sh.executing = std::this_thread::get_id();
    }
    ~Executing() {
      std::lock_guard<std::mutex> lock(sh.mutex);
      sh.executing = previous;
      if (previous == std::thread::id{}) {
        if (reply)
          for (auto& w : sh.pendingWarnings) reply->lines.push_back("warning: " + w);
        sh.pendingWarnings.clear();
      }
    }
  };
  {
    Executing guard(*m_shared);
    guard.reply = &r;
    // Every command is one undo step; the interpreter's own state (journal,
    // filter and style texts) is kept per step so that undo restores it too.
    struct UndoGroup {
      session::Session& s;
      UndoGroup(session::Session& session, const std::string& label) : s(session) { s.BeginUndoGroup(label); }
      ~UndoGroup() { s.EndUndoGroup(); }
    };
    const bool isUndo = trimmed == "undo" || trimmed == "redo";
    const auto generation = m_session.UndoGeneration();
    Snapshot before{m_journal, m_filters, m_styles};
    try {
      line.words = Words(line.raw);
      if (isUndo) {
        r = Dispatch(line);
      } else {
        UndoGroup group(m_session, trimmed);
        r = Dispatch(line);
      }
    } catch (const ParseError& e) {
      r.ok = false;
      r.errors.push_back(e.what());
    } catch (const CommandError& e) {
      r.ok = false;
      r.errors.push_back(e.what());
    } catch (const std::exception& e) {
      r.ok = false;
      r.errors.push_back(e.what());
    }
    if (!isUndo) {
      if (m_session.UndoGeneration() != generation) {  // this command became an undo step
        m_past.push_back(std::move(before));
        m_future.clear();
      }
      while (m_past.size() > m_session.UndoDepth()) m_past.erase(m_past.begin());
    }
  }  // pending warnings are appended to r here
  return r;
}

Reply Interpreter::Run(std::string_view script, bool stopOnError) {
  Reply all;
  std::istringstream in{std::string(script)};
  std::string physical, logical;
  int number = 0, start = 0;
  while (std::getline(in, physical)) {
    ++number;
    if (!physical.empty() && physical.back() == '\r') physical.pop_back();
    if (logical.empty()) start = number;
    else physical.erase(0, std::min(physical.size(), physical.find_first_not_of(" \t")));  // continuation indent
    if (!physical.empty() && physical.back() == '\\') {
      physical.pop_back();
      logical += physical.substr(0, physical.find_last_not_of(" \t") + 1) + " ";
      continue;
    }
    logical += physical;
    Reply r = Execute(logical);
    logical.clear();
    all.lines.insert(all.lines.end(), r.lines.begin(), r.lines.end());
    for (const auto& e : r.errors) all.errors.push_back("line " + std::to_string(start) + ": " + e);
    if (!r.ok) {
      all.ok = false;
      if (stopOnError) return all;
    }
    if (r.quit) {
      all.quit = true;
      return all;
    }
  }
  if (!logical.empty()) {
    Reply r = Execute(logical);
    all.lines.insert(all.lines.end(), r.lines.begin(), r.lines.end());
    for (const auto& e : r.errors) all.errors.push_back("line " + std::to_string(start) + ": " + e);
    all.ok = all.ok && r.ok;
  }
  return all;
}

namespace {

/** Marks a parse error inside a rest-of-line argument with a caret line. */
[[noreturn]] void Rethrow(const ParseError& e, const std::string& raw, std::size_t offset) {
  const std::size_t col = offset + e.Column();
  throw CommandError(e.Message() + " at column " + std::to_string(col) + "\n  " + raw + "\n  " +
                     std::string(col > 0 ? col - 1 : 0, ' ') + "^");
}

void Check(const changes::Result& r) {
  if (!r.ok) throw CommandError(r.messages.empty() ? std::string("change failed") : Join(r.messages, "; "));
}

}  // namespace

Reply Interpreter::Dispatch(Line& line) {
  Reply r;
  const std::string& cmd = line.W(0);
  auto need = [&](std::size_t n, const char* usage) {
    if (line.Size() < n) throw CommandError(std::string("usage: ") + usage);
  };
  auto journal = [&] { m_journal.push_back(Trim(line.raw)); };
  auto& session = m_session;

  // ---- helpers bound to the session ----------------------------------------
  auto requireScene = [&](const std::string& name) {
    if (!session.HasScene(name)) {
      throw CommandError("unknown scene '" + name + "'" +
                         (session.SceneNames().empty() ? std::string() : "; scenes: " + Join(session.SceneNames(), ", ")));
    }
    return session.FindScene(name);
  };
  struct LayerRef {
    std::string scene, layer;
    ElementKind kind;
  };
  auto layerRef = [&](const std::string& path) {
    const auto slash = path.find('/');
    if (slash == std::string::npos) throw CommandError("expected <scene>/<layer>, got '" + path + "'");
    LayerRef ref{path.substr(0, slash), path.substr(slash + 1), ElementKind::Point};
    auto scene = requireScene(ref.scene);
    const auto types = scene->layers.Types();
    auto it = types.find(ref.layer);
    if (it == types.end()) {
      std::vector<std::string> names;
      for (const auto& [n, t] : types) names.push_back(n);
      throw CommandError("scene '" + ref.scene + "' has no layer '" + ref.layer + "'" +
                         (names.empty() ? std::string() : "; layers: " + Join(names, ", ")));
    }
    const auto kind = data::KindOf(it->second);
    if (!kind) throw CommandError("layer '" + path + "' has an element type the shell does not know");
    ref.kind = *kind;
    return ref;
  };
  struct SeriesHit {
    data::SeriesRef ref;
    ElementKind kind;
    data::SeriesInfo info;
  };
  auto findSeries = [&](const std::string& spec) {
    std::string name = spec, source;
    if (const auto at = spec.rfind('@'); at != std::string::npos && at > 0) {
      name = spec.substr(0, at);
      source = spec.substr(at + 1);
    }
    std::vector<SeriesHit> hits;
    for (const auto& info : session.Data().List()) {
      if (info.name != name || (!source.empty() && info.source != source)) continue;
      if (auto k = data::KindOf(info.type)) hits.push_back({{info.name, info.source}, *k, info});
    }
    if (hits.empty()) throw CommandError("unknown series '" + spec + "' (see: series)");
    if (hits.size() > 1) {
      std::vector<std::string> alts;
      for (const auto& h : hits) alts.push_back(h.ref.name + "@" + h.ref.source);
      throw CommandError("series '" + spec + "' is ambiguous: " + Join(alts, ", "));
    }
    if (source.empty()) hits[0].ref.source.clear();  // unambiguous by name: keep the short reference
    return hits[0];
  };
  auto options = [&](std::size_t from, bool allowPositional) {
    adapters::Options o;
    for (std::size_t k = from; k < line.Size(); ++k) {
      const auto& w = line.W(k);
      const auto eq = w.find('=');
      if (eq == std::string::npos || eq == 0) {
        if (allowPositional) continue;
        throw CommandError("expected key=value, got '" + w + "'");
      }
      o[w.substr(0, eq)] = w.substr(eq + 1);
    }
    return o;
  };

  // ---- commands --------------------------------------------------------------
  if (cmd == "quit" || cmd == "exit") {
    r.quit = true;
    return r;
  }
  if (cmd == "echo") {
    r.lines.push_back(line.Rest(1));
    return r;
  }
  if (cmd == "help") {
    std::string topic = line.Rest(1);
    std::istringstream h(Help(topic));
    for (std::string l; std::getline(h, l);) r.lines.push_back(l);
    return r;
  }
  if (cmd == "undo" || cmd == "redo") {
    need(1, cmd.c_str());
    const bool undo = cmd == "undo";
    const std::string label = undo ? session.UndoLabel() : session.RedoLabel();
    auto& from = undo ? m_past : m_future;
    auto& to = undo ? m_future : m_past;
    const bool aligned = from.size() == (undo ? session.UndoDepth() : session.RedoDepth());
    const auto result = undo ? session.Undo() : session.Redo();
    if (!result.ok) {
      if (result.applied == 0 && !result.messages.empty() && result.messages.front().rfind("nothing to", 0) == 0)
        throw CommandError(result.messages.front());
      m_past.clear();
      m_future.clear();
      throw CommandError(cmd + " failed: " + (result.messages.empty() ? std::string("?") : result.messages.front()));
    }
    if (aligned && !from.empty()) {
      to.push_back({m_journal, m_filters, m_styles});
      m_journal = std::move(from.back().journal);
      m_filters = std::move(from.back().filters);
      m_styles = std::move(from.back().styles);
      from.pop_back();
    } else {
      m_past.clear();
      m_future.clear();
    }
    r.lines.push_back(std::string(undo ? "undone: " : "redone: ") + label);
    return r;
  }

  if (cmd == "load") {
    need(3, "load <adapter> <path> [key=value ...]");
    auto adapter = adapters::Registry::Global().Create(line.W(1));
    if (!adapter)
      throw CommandError("unknown adapter '" + line.W(1) + "'; available: " + Join(adapters::Registry::Global().Names(), ", "));
    const std::string path = Resolve(line.W(2));
    const auto opts = options(3, false);
    adapters::LoadReport report;
    try {
      report = adapter->Open(session.Data(), path, opts);
    } catch (const std::invalid_argument& e) {
      throw CommandError(e.what());
    }
    session.DataChanged();
    std::istringstream rep(report.ToString());
    for (std::string l; std::getline(rep, l);) r.lines.push_back(l);
    if (!report.Ok()) {
      r.ok = false;
      r.errors.push_back("nothing registered from '" + line.W(2) + "'");
      return r;
    }
    m_loads.push_back({line.W(1), path, opts});
    std::string canonical = "load " + QuoteWord(line.W(1)) + " " + QuoteWord(path);
    for (const auto& [k, v] : opts) canonical += " " + QuoteWord(k + "=" + v);
    m_journal.push_back(canonical);
    return r;
  }

  if (cmd == "series") {
    const std::string pattern = line.Rest(1).empty() ? "*" : line.Rest(1);
    for (const auto& info : session.Data().List()) {
      if (!Glob(pattern, info.name)) continue;
      const auto k = data::KindOf(info.type);
      r.lines.push_back(info.name + "  " + (k ? KindName(*k) : std::string("?")) + "  " + std::to_string(info.entries) +
                        (info.entries == 1 ? " entry" : " entries") + "  " + info.source);
    }
    if (r.lines.empty()) r.lines.push_back("(no series)");
    return r;
  }

  if (cmd == "describe") {
    need(2, "describe <series>");
    const auto hit = findSeries(line.W(1));
    r.lines.push_back(hit.info.name + " (" + KindName(hit.kind) + ")");
    r.lines.push_back("  source: " + hit.info.source);
    r.lines.push_back("  entries: " + std::to_string(hit.info.entries));
    if (!hit.info.description.empty()) r.lines.push_back("  description: " + hit.info.description);
    WithKind(hit.kind, [&]<typename X>() {
      if (hit.info.entries == 0) return;
      auto entry = session.Data().Entry<X>({hit.info.name, hit.info.source}, 0);
      if (!entry) return;
      std::map<std::string, std::pair<std::string, std::size_t>> keys;  // key -> type, count
      for (const auto& e : *entry)
        for (const auto& [k, v] : e.meta.All()) {
          const char* t = std::holds_alternative<bool>(v) ? "flag"
                          : std::holds_alternative<std::int64_t>(v) ? "integer"
                          : std::holds_alternative<double>(v) ? "number" : "text";
          auto& slot = keys[k];
          if (slot.first.empty()) slot.first = t;
          else if (slot.first != t) slot.first = "mixed";
          ++slot.second;
        }
      r.lines.push_back("  entry 0: " + std::to_string(entry->size()) + " elements");
      for (const auto& [k, tc] : keys)
        r.lines.push_back("    " + k + "  " + tc.first + "  (" + std::to_string(tc.second) + " of " +
                          std::to_string(entry->size()) + ")");
    });
    return r;
  }

  if (cmd == "scene") {
    need(2, "scene new|rm|ls|show ...");
    const auto& sub = line.W(1);
    if (sub == "ls") {
      for (const auto& n : session.SceneNames()) r.lines.push_back(n);
      if (r.lines.empty()) r.lines.push_back("(no scenes)");
      return r;
    }
    if (sub == "new") {
      need(3, "scene new <name> ...");
      Script s;
      for (std::size_t k = 2; k < line.Size(); ++k) {
        s.Add(changes::CreateScene{line.W(k)});
        s.Add(changes::SetCamera{line.W(k), std::make_shared<scene::camera::Camera>(line.W(k))});
      }
      Check(s.Run(session));
      journal();
      return r;
    }
    if (sub == "rm") {
      need(3, "scene rm <name> ...");
      Script s;
      for (std::size_t k = 2; k < line.Size(); ++k) {
        requireScene(line.W(k));
        s.Add(changes::RemoveScene{line.W(k)});
      }
      Check(s.Run(session));
      for (std::size_t k = 2; k < line.Size(); ++k) {
        const std::string prefix = line.W(k) + "/";
        for (auto it = m_filters.begin(); it != m_filters.end();) it = it->first.rfind(prefix, 0) == 0 ? m_filters.erase(it) : std::next(it);
        for (auto it = m_styles.begin(); it != m_styles.end();) it = it->first.rfind(prefix, 0) == 0 ? m_styles.erase(it) : std::next(it);
      }
      journal();
      return r;
    }
    if (sub == "show") {
      need(3, "scene show <name>");
      auto scene = requireScene(line.W(2));
      r.lines.push_back("scene " + line.W(2));
      if (scene->camera) {
        const auto& c = *scene->camera;
        auto vec = [](const std::vector<double>& v) {
          std::string s;
          for (std::size_t i = 0; i < v.size(); ++i) s += (i ? " " : "") + Num(v[i]);
          return s;
        };
        std::string mode = "free";
        if (scene->cameraOptions.mode == scene::camera::CameraMode::Fixed) mode = "fixed";
        if (scene->cameraOptions.mode == scene::camera::CameraMode::Animated) mode = "animated";
        r.lines.push_back("  camera: dir " + vec(c.directionVector) + ", pos " + vec(c.positionVector) + ", zoom " +
                          Num(c.magnification) + ", mode " + mode);
      }
      for (const auto& [name, type] : scene->layers.Types()) {
        const auto kind = data::KindOf(type);
        std::string desc = "  layer " + name + " (" + (kind ? KindName(*kind) : std::string("?")) + ")";
        if (kind)
          WithKind(*kind, [&]<typename X>() {
            const auto* l = scene->layers.Find<X>(name);
            if (!l) return;
            if (l->series) desc += ": series " + l->series->name + ", entry " + std::to_string(l->entry.value_or(0));
          });
        r.lines.push_back(desc);
        const std::string key = line.W(2) + "/" + name;
        if (auto f = m_filters.find(key); f != m_filters.end()) r.lines.push_back("    where " + f->second);
        if (auto s = m_styles.find(key); s != m_styles.end())
          for (const auto& rule : s->second) r.lines.push_back("    style " + rule);
      }
      return r;
    }
    throw CommandError("unknown scene command '" + sub + "' (new, rm, ls, show)");
  }

  if (cmd == "layer") {
    need(2, "layer add|rm|ls ...");
    const auto& sub = line.W(1);
    if (sub == "ls") {
      need(3, "layer ls <scene>");
      auto scene = requireScene(line.W(2));
      for (const auto& [name, type] : scene->layers.Types()) {
        const auto k = data::KindOf(type);
        r.lines.push_back(name + "  " + (k ? KindName(*k) : std::string("?")));
      }
      if (r.lines.empty()) r.lines.push_back("(no layers)");
      return r;
    }
    if (sub == "add") {
      need(4, "layer add <scene> <series> [as <layer>] [entry <n>]");
      requireScene(line.W(2));
      const auto hit = findSeries(line.W(3));
      std::string name = hit.ref.name;
      std::size_t entry = 0;
      for (std::size_t k = 4; k < line.Size(); k += 2) {
        if (k + 1 >= line.Size()) throw CommandError("'" + line.W(k) + "' needs a value");
        if (line.W(k) == "as") {
          name = line.W(k + 1);
        } else if (line.W(k) == "entry") {
          const auto n = ToNumber(line.W(k + 1));
          if (!n || *n < 0 || *n != std::floor(*n)) throw CommandError("entry must be a non-negative integer");
          entry = static_cast<std::size_t>(*n);
        } else {
          throw CommandError("unexpected '" + line.W(k) + "' (as, entry)");
        }
      }
      if (entry >= hit.info.entries && hit.info.entries > 0)
        throw CommandError("entry " + std::to_string(entry) + " out of range (series has " +
                           std::to_string(hit.info.entries) + ")");
      WithKind(hit.kind, [&]<typename X>() { Check(session.Resolve(changes::AddLayer<X>{line.W(2), name, hit.ref, entry})); });
      journal();
      return r;
    }
    if (sub == "rm") {
      need(3, "layer rm <scene>/<layer>");
      const auto ref = layerRef(line.W(2));
      Check(session.Resolve(changes::RemoveLayer{ref.scene, ref.layer}));
      m_filters.erase(line.W(2));
      m_styles.erase(line.W(2));
      journal();
      return r;
    }
    throw CommandError("unknown layer command '" + sub + "' (add, rm, ls)");
  }

  if (cmd == "where") {
    need(3, "where <scene>/<layer> <filter-expression>");
    const auto ref = layerRef(line.W(1));
    const std::string expr = line.Rest(2);
    stdlib::Selector sel = stdlib::All();
    try {
      sel = ParseFilter(expr);
    } catch (const ParseError& e) {
      Rethrow(e, line.raw, line.Offset(2));
    }
    WithKind(ref.kind, [&]<typename X>() {
      Check(session.Resolve(changes::SetLayerFilter<X>{ref.scene, ref.layer,
                                                       std::make_shared<filter::Filter<X>>(sel.For<X>())}));
    });
    m_filters[line.W(1)] = expr;
    journal();
    return r;
  }

  if (cmd == "style") {
    need(3, "style <scene>/<layer> [when <filter> then] key=value ... | preset <name> [args]");
    const auto ref = layerRef(line.W(1));
    if (line.W(2) == "preset") {
      need(4, "style <scene>/<layer> preset <name> [args]");
      const std::string preset = line.W(3);
      auto num = [&](std::size_t k, double fallback) {
        if (k >= line.Size()) return fallback;
        auto v = ToNumber(line.W(k));
        if (!v) throw CommandError("expected a number, got '" + line.W(k) + "'");
        return *v;
      };
      auto text = [&](std::size_t k, const std::string& fallback) { return k < line.Size() ? line.W(k) : fallback; };
      Script s;
      auto addSheet = [&]<typename X>(const style::StyleSheet<X>& sheet) {
        s.Add(changes::AddStyleRule<X>{ref.scene, ref.layer, {}, sheet.base});
        for (const auto& rule : sheet.rules) s.Add(changes::AddStyleRule<X>{ref.scene, ref.layer, rule.when, rule.style});
      };
      auto wrongKind = [&](const char* kind) {
        throw CommandError("preset '" + preset + "' applies to " + kind + " layers; '" + line.W(1) + "' shows " +
                           KindName(ref.kind) + "s");
      };
      if (preset == "hits-by-energy") {
        if (ref.kind != ElementKind::Point) wrongKind("point");
        addSheet(stdlib::HitsByEnergy({num(4, 0), num(5, 50)}, text(6, stdlib::kEnergy)));
      } else if (preset == "tracks-by-charge") {
        if (ref.kind != ElementKind::Line) wrongKind("line");
        addSheet(stdlib::TracksByCharge(text(4, "charge")));
      } else if (preset == "detector-outline") {
        if (ref.kind != ElementKind::Surface) wrongKind("surface");
        addSheet(stdlib::DetectorOutline());
      } else if (preset == "ghosted") {
        WithKind(ref.kind, [&]<typename X>() { addSheet(stdlib::Ghosted<X>()); });
      } else if (preset == "color-by") {
        need(7, "style <scene>/<layer> preset color-by <attribute> <lo> <hi> [bins]");
        WithKind(ref.kind, [&]<typename X>() {
          for (const auto& rule : stdlib::ColorByAttribute<X>(line.W(4), stdlib::Monochrome(), {num(5, 0), num(6, 1)},
                                                              static_cast<std::size_t>(num(7, 16))))
            s.Add(changes::AddStyleRule<X>{ref.scene, ref.layer, rule.when, rule.style});
        });
      } else if (preset == "size-by") {
        need(9, "style <scene>/<layer> preset size-by <attribute> <min-size> <max-size> <lo> <hi> [bins]");
        WithKind(ref.kind, [&]<typename X>() {
          for (const auto& rule : stdlib::SizeByAttribute<X>(line.W(4), static_cast<float>(num(5, 1)),
                                                             static_cast<float>(num(6, 5)), {num(7, 0), num(8, 1)},
                                                             static_cast<std::size_t>(num(9, 8))))
            s.Add(changes::AddStyleRule<X>{ref.scene, ref.layer, rule.when, rule.style});
        });
      } else {
        throw CommandError("unknown preset '" + preset +
                           "' (hits-by-energy, tracks-by-charge, detector-outline, ghosted, color-by, size-by)");
      }
      Check(s.Run(session));
      m_styles[line.W(1)].push_back(line.Rest(2));
      journal();
      return r;
    }
    // [when <filter> then] assignments
    std::size_t assignFrom = 2;
    std::optional<stdlib::Selector> when;
    if (line.W(2) == "when") {
      std::size_t then = 0;
      for (std::size_t k = 3; k < line.Size(); ++k)
        if (line.W(k) == "then") {
          then = k;
          break;
        }
      if (!then) throw CommandError("'when <filter>' needs 'then' before the style");
      const std::size_t a = line.Offset(3), b = line.words[then].begin;
      const std::string expr = Trim(line.raw.substr(a, b - a));
      try {
        when = ParseFilter(expr);
      } catch (const ParseError& e) {
        Rethrow(e, line.raw, a);
      }
      assignFrom = then + 1;
    }
    const std::string assignments = line.Rest(assignFrom);
    if (assignments.empty()) throw CommandError("no style given (key=value ...)");
    try {
      WithKind(ref.kind, [&]<typename X>() {
        style::StyleOf<X> st;
        if constexpr (std::is_same_v<X, PE>) st = ParsePointStyle(assignments);
        else if constexpr (std::is_same_v<X, LE>) st = ParseLineStyle(assignments);
        else st = ParseSurfaceStyle(assignments);
        filter::Filter<X> f = when ? when->template For<X>() : filter::Filter<X>{};
        Check(session.Resolve(changes::AddStyleRule<X>{ref.scene, ref.layer, f, st}));
      });
    } catch (const ParseError& e) {
      Rethrow(e, line.raw, line.Offset(assignFrom));
    }
    m_styles[line.W(1)].push_back(line.Rest(2));
    journal();
    return r;
  }

  if (cmd == "env") {
    need(3, "env <scene> key=value ... | env <scene> preset publication|default");
    requireScene(line.W(1));
    style::EnvironmentStyle env;
    if (line.W(2) == "preset") {
      need(4, "env <scene> preset publication|default");
      if (line.W(3) == "publication") env = stdlib::Publication();
      else if (line.W(3) == "default") env = stdlib::DefaultEnvironment();
      else throw CommandError("unknown environment preset '" + line.W(3) + "' (publication, default)");
    } else {
      try {
        env = ParseEnvironment(line.Rest(2));
      } catch (const ParseError& e) {
        Rethrow(e, line.raw, line.Offset(2));
      }
    }
    Check(session.Resolve(changes::SetEnvironment{line.W(1), env}));
    journal();
    return r;
  }

  if (cmd == "camera") {
    need(3, "camera <scene> dir|pos <x> <y> <z> | zoom <m> | reset | mode <mode> | share <scene>");
    requireScene(line.W(1));
    const auto& sub = line.W(2);
    auto vec3 = [&]() {
      need(6, "camera <scene> dir|pos <x> <y> <z>");
      std::vector<double> v;
      for (std::size_t k = 3; k < 6; ++k) {
        auto n = ToNumber(line.W(k));
        if (!n) throw CommandError("expected a number, got '" + line.W(k) + "'");
        v.push_back(*n);
      }
      return v;
    };
    if (sub == "dir") Check(session.Resolve(changes::ChangeCameraDirection{line.W(1), vec3()}));
    else if (sub == "pos") Check(session.Resolve(changes::ChangeCameraPosition{line.W(1), vec3()}));
    else if (sub == "zoom") {
      need(4, "camera <scene> zoom <m>");
      auto m = ToNumber(line.W(3));
      if (!m) throw CommandError("expected a number, got '" + line.W(3) + "'");
      Check(session.Resolve(changes::ChangeCameraMagnification{line.W(1), *m}));
    } else if (sub == "reset") {
      Script s;
      s.Add(changes::ChangeCameraPosition{line.W(1), {0, 0, 0}});
      s.Add(changes::ChangeCameraDirection{line.W(1), {0, 0, 1}});
      s.Add(changes::ResetCameraMagnification{line.W(1)});
      Check(s.Run(session));
    } else if (sub == "mode") {
      need(4, "camera <scene> mode free|fixed|animated");
      scene::camera::CameraOptions o;
      if (line.W(3) == "free") o.mode = scene::camera::CameraMode::Free;
      else if (line.W(3) == "fixed") o.mode = scene::camera::CameraMode::Fixed;
      else if (line.W(3) == "animated") o.mode = scene::camera::CameraMode::Animated;
      else throw CommandError("camera mode must be free, fixed or animated");
      Check(session.Resolve(changes::SetCameraOptions{line.W(1), o}));
    } else if (sub == "share") {
      need(4, "camera <scene> share <other-scene>");
      auto other = requireScene(line.W(3));
      if (!other->camera) throw CommandError("scene '" + line.W(3) + "' has no camera");
      Check(session.Resolve(changes::SetCamera{line.W(1), other->camera}));
    } else {
      throw CommandError("unknown camera command '" + sub + "' (dir, pos, zoom, reset, mode, share)");
    }
    journal();
    return r;
  }

  if (cmd == "layout") {
    need(2, "layout <expression> | layout show");
    if (line.Size() == 2 && line.W(1) == "show") {
      const auto& l = session.Options().sceneLayout;
      r.lines.push_back(l ? FormatLayout(*l) : std::string("(no layout)"));
      return r;
    }
    scene::layout::LayoutNode node;
    try {
      node = ParseLayout(line.Rest(1));
    } catch (const ParseError& e) {
      Rethrow(e, line.raw, line.Offset(1));
    }
    Check(session.SetLayout(node));
    for (const auto& problem : session.ValidateLayout()) r.lines.push_back("warning: " + problem);
    journal();
    return r;
  }

  if (cmd == "entry") {
    need(3, "entry <scene>/<layer> <n>|next|prev|first|last");
    const auto ref = layerRef(line.W(1));
    WithKind(ref.kind, [&]<typename X>() {
      const auto* l = session.FindScene(ref.scene)->layers.Find<X>(ref.layer);
      const std::size_t size = l->series ? session.Data().Size<X>(*l->series) : 0;
      const std::size_t cur = l->entry.value_or(0);
      std::size_t next = cur;
      const auto& w = line.W(2);
      if (w == "next") next = cur + 1;
      else if (w == "prev") next = cur == 0 ? 0 : cur - 1;
      else if (w == "first") next = 0;
      else if (w == "last") next = size == 0 ? 0 : size - 1;
      else {
        auto n = ToNumber(w);
        if (!n || *n < 0 || *n != std::floor(*n)) throw CommandError("expected an entry number, next, prev, first or last");
        next = static_cast<std::size_t>(*n);
      }
      if (next >= size)
        throw CommandError("entry " + std::to_string(next) + " out of range (" + std::to_string(size) + " entries)");
      Check(session.Resolve(changes::SetEntry<X>{ref.scene, ref.layer, next}));
      r.lines.push_back(line.W(1) + ": entry " + std::to_string(next) + " of " + std::to_string(size));
      m_journal.push_back("entry " + line.W(1) + " " + std::to_string(next));
    });
    return r;
  }

  if (cmd == "backend") {
    need(2, "backend init|rm|ls|flush ...");
    const auto& sub = line.W(1);
    if (sub == "ls") {
      for (const auto& id : session.BackendIds()) {
        const auto scenes = session.OpenScenes(id);
        const std::vector<std::string> open(scenes.begin(), scenes.end());
        r.lines.push_back(id + (session.IsRunning(id) ? "" : " (stopped)") + ": " +
                          (open.empty() ? std::string("no scenes open") : Join(open, ", ")));
      }
      if (r.lines.empty()) r.lines.push_back("(no backends)");
      return r;
    }
    if (sub == "init") {
      need(3, "backend init <type> [id] [key=value ...]");
      std::string id;
      std::size_t from = 3;
      if (line.Size() > 3 && line.W(3).find('=') == std::string::npos) {
        id = line.W(3);
        from = 4;
      }
      auto opts = options(from, false);
      {
        std::lock_guard<std::mutex> lock(m_shared->mutex);
        m_shared->captureUrls = true;
        m_shared->pendingUrls.clear();
      }
      const auto result = session.InitBackend(line.W(2), id, opts);
      {
        std::lock_guard<std::mutex> lock(m_shared->mutex);
        m_shared->captureUrls = false;
        for (auto& u : m_shared->pendingUrls) r.lines.push_back(u);  // e.g. the page of a web viewer
        m_shared->pendingUrls.clear();
      }
      Check(result);
      return r;
    }
    if (sub == "rm") {
      need(3, "backend rm <id>");
      Check(session.DetachBackend(line.W(2)));
      return r;
    }
    if (sub == "flush") {
      Check(session.Flush(line.W(2)));
      return r;
    }
    throw CommandError("unknown backend command '" + sub + "' (init, rm, ls, flush)");
  }

  if (cmd == "open" || cmd == "close") {
    need(3, cmd == "open" ? "open <backend> <scene> ..." : "close <backend> <scene> ...");
    for (std::size_t k = 2; k < line.Size(); ++k)
      Check(cmd == "open" ? session.Open(line.W(1), line.W(k)) : session.Close(line.W(1), line.W(k)));
    return r;
  }

  if (cmd == "log") {
    std::size_t n = 20;
    if (line.Size() > 1) {
      auto v = ToNumber(line.W(1));
      if (!v || *v < 0) throw CommandError("expected a count");
      n = static_cast<std::size_t>(*v);
    }
    std::lock_guard<std::mutex> lock(m_shared->mutex);
    const auto& log = m_shared->log;
    for (std::size_t i = log.size() > n ? log.size() - n : 0; i < log.size(); ++i) {
      const char* sev = log[i].first == changes::Severity::Error ? "error" : log[i].first == changes::Severity::Warning ? "warning" : "info";
      r.lines.push_back(std::string(sev) + ": " + log[i].second);
    }
    return r;
  }

  if (cmd == "save") {
    need(2, "save <file>");
    const std::string path = line.W(1);  // outputs are relative to the working directory
    std::ofstream out(path, std::ios::binary);
    const auto cmds = StateCommands();
    const std::string json = WriteConfig(cmds);
    out << json;
    if (!out) throw CommandError("cannot write '" + path + "'");
    r.lines.push_back("saved " + std::to_string(cmds.size()) + " commands to " + line.W(1));
    return r;
  }

  if (cmd == "source") {
    need(2, "source <file>");
    const std::string path = Resolve(line.W(1));
    std::ifstream in(path, std::ios::binary);
    if (!in) throw CommandError("cannot read '" + path + "'");
    std::stringstream ss;
    ss << in.rdbuf();
    std::string text = ss.str();
    if (m_sourceDepth >= 8) throw CommandError("source nested too deeply");
    // A saved configuration is JSON; anything else is a script.
    if (IsConfig(text)) {
      std::vector<std::string> cmds;
      try {
        cmds = ReadConfig(text);
      } catch (const ConfigError& e) {
        throw CommandError(std::string(e.what()) + " ('" + line.W(1) + "')");
      }
      text.clear();
      for (const auto& c : cmds) {
        if (c.find('\n') != std::string::npos) throw CommandError("configuration commands must be single lines");
        text += c + "\n";
      }
    }
    const std::string savedBase = m_base;
    m_base = std::filesystem::path(path).parent_path().string();  // relative names inside resolve against the file
    ++m_sourceDepth;
    Reply inner;
    try {
      inner = Run(text, m_stopOnError);
    } catch (...) {
      --m_sourceDepth;
      m_base = savedBase;
      throw;
    }
    --m_sourceDepth;
    m_base = savedBase;
    r = inner;
    return r;
  }

  throw CommandError("unknown command '" + cmd + "' (try: help)");
}

std::vector<std::string> Interpreter::StateCommands() const {
  // Camera placement commands are replaced by the final camera of each scene
  // (which also covers moves made by backends); mode and share are kept.
  std::vector<std::string> out;
  for (const auto& c : m_journal) {
    const auto w = SplitWords(c);
    if (w.size() >= 3 && w[0] == "camera" && (w[2] == "dir" || w[2] == "pos" || w[2] == "zoom" || w[2] == "reset")) continue;
    out.push_back(c);
  }
  for (const auto& name : m_session.SceneNames()) {
    auto scene = m_session.FindScene(name);
    if (!scene || !scene->camera) continue;
    const auto& c = *scene->camera;
    auto vec = [](const std::vector<double>& v) {
      std::string s;
      for (std::size_t i = 0; i < v.size(); ++i) s += " " + Num(v[i]);
      return s;
    };
    if (c.directionVector.size() == 3) out.push_back("camera " + QuoteWord(name) + " dir" + vec(c.directionVector));
    if (c.positionVector.size() == 3) out.push_back("camera " + QuoteWord(name) + " pos" + vec(c.positionVector));
    out.push_back("camera " + QuoteWord(name) + " zoom " + Num(c.magnification));
  }
  return out;
}

// ---------------------------------------------------------------------------
// Help (generated from the command table and the registries)
// ---------------------------------------------------------------------------
std::string Interpreter::Help(const std::string& topicIn) const {
  const std::string topic = Trim(topicIn);
  std::string s;
  if (topic.empty()) {
    s += "Commands:\n";
    std::size_t width = 0;
    for (const auto& f : Forms()) width = std::max(width, f.usage.size());
    width = std::min<std::size_t>(width, 48);
    for (const auto& f : Forms()) {
      std::string u = f.usage;
      if (u.size() < width) u += std::string(width - u.size(), ' ');
      s += "  " + u + "  " + f.summary + "\n";
    }
    s += "Topics: help filters | styles | layouts | adapters | backends | adapter <name> | backend <type>\n";
    return s;
  }
  const auto words = SplitWords(topic);
  if (words[0] == "adapter" && words.size() > 1) {
    const auto h = adapters::Registry::Global().Help(words[1]);
    if (h.empty()) return "unknown adapter '" + words[1] + "'; available: " + Join(adapters::Registry::Global().Names(), ", ") + "\n";
    return h;
  }
  if (words[0] == "backend" && words.size() > 1) {
    const auto h = backend::Registry::Global().Help(words[1]);
    if (h.empty()) return "unknown backend '" + words[1] + "'; available: " + Join(backend::Registry::Global().Names(), ", ") + "\n";
    return h;
  }
  if (words[0] == "adapters") {
    for (const auto& n : adapters::Registry::Global().Names()) s += adapters::Registry::Global().Help(n);
    return s.empty() ? "(no adapters)\n" : s;
  }
  if (words[0] == "backends") {
    for (const auto& n : backend::Registry::Global().Names())
      if (n.rfind("test-", 0) != 0) s += backend::Registry::Global().Help(n);
    return s.empty() ? "(no backends)\n" : s;
  }
  if (words[0] == "filters") {
    return "Filter expressions (where, style ... when):\n"
           "  attribute == != < <= > >= value     value: number, \"text\", true, false\n"
           "  attribute in [v1, v2, ...]          attribute ~ \"regex\"\n"
           "  not X    X and Y    X or Y    ( X )\n"
           "  all()  none()  exists(attr)  first(n)  index(i)  between(attr, lo, hi)\n"
           "  inside(shape[, any|all|centroid])  outside(shape[, ...])  within(d, shape[, ...])\n"
           "  shapes (mm): box(dx, dy, dz)  sphere(r)  cylinder(rmin, rmax, halfz)\n"
           "               cone(rmin1, rmax1, rmin2, rmax2, halfz)  translate(shape, x, y, z)\n"
           "  A missing attribute fails every comparison.\n";
  }
  if (words[0] == "styles") {
    s = "Style keys (style <scene>/<layer> [when <filter> then] key=value ...):\n";
    for (const char* kind : {"point", "line", "surface", "environment"})
      s += std::string("  ") + kind + ": " + Join(StyleKeys(kind), " ") + "\n";
    s += "  colours: #rgb, #rrggbb, #rrggbbaa or accent black blue cyan gray green magenta orange purple red white yellow\n"
         "  marker: circle square diamond triangle cross    dash: solid dashed dotted dashdot\n"
         "Presets (style <scene>/<layer> preset <name> [args]):\n"
         "  hits-by-energy [lo hi [attribute]]   tracks-by-charge [attribute]   detector-outline   ghosted\n"
         "  color-by <attribute> <lo> <hi> [bins]   size-by <attribute> <min> <max> <lo> <hi> [bins]\n"
         "Environment presets (env <scene> preset <name>): publication default\n";
    return s;
  }
  if (words[0] == "layouts") {
    return "Layout expressions (layout ...):\n"
           "  <scene>                 one pane\n"
           "  split(a, b, ...)        side by side      vsplit(a, b, ...)   stacked\n"
           "  tabs(a, b, ...)         alternatives (static backends show the first)\n"
           "  child options: as \"label\"   *weight (splits)\n"
           "  example: split(xy as \"Front\" *2, tabs(rz, xz))\n";
  }
  for (const auto& f : Forms())
    if (f.words[0] == words[0]) s += "  " + f.usage + "\n      " + f.summary + "\n";
  if (s.empty()) return "unknown help topic '" + topic + "'\n" + Help();
  return s;
}

std::string Interpreter::Reference(const std::string& section) {
  auto md = [](const std::string& in) {  // escape table cells
    std::string out;
    for (char c : in) out += c == '|' ? std::string("\\|") : std::string(1, c);
    return out;
  };
  auto join = [](const auto& items, auto&& f) {
    std::string out;
    for (const auto& i : items) out += (out.empty() ? "" : ", ") + f(i);
    return out.empty() ? std::string("none") : out;
  };
  std::string s;
  if (section.empty() || section == "commands") {
    s += "## Commands\n\n| Usage | Description |\n|---|---|\n";
    for (const auto& f : Forms()) s += "| `" + md(f.usage) + "` | " + md(f.summary) + " |\n";
    s += "\n## Expression languages\n\nThe shell's own `help filters`, `help styles` and `help layouts`:\n\n";
    session::Session scratch("reference");
    Interpreter in(scratch);
    for (const char* topic : {"filters", "styles", "layouts"}) s += "```text\n" + in.Help(topic) + "```\n\n";
  }
  if (section.empty() || section == "adapters") {
    s += "## Adapters\n\n";
    const auto names = adapters::Registry::Global().Names();
    if (names.empty()) s += "No adapter is enabled in this build.\n\n";
    for (const auto& n : names) {
      const auto c = adapters::Registry::Global().Create(n)->GetCapabilities();
      s += "### " + n + "\n\n" + c.description + "\n\n| | |\n|---|---|\n";
      s += "| Element kinds | " + join(c.kinds, [](data::ElementKind k) { return std::string(data::ToString(k)); }) + " |\n";
      s += "| File types | " + join(c.extensions, [](const std::string& e) { return "`" + e + "`"; }) + " |\n\n";
      if (!c.options.empty()) {
        auto opts = c.options;
        std::sort(opts.begin(), opts.end(), [](const auto& x, const auto& y) { return x.name < y.name; });
        s += "| Option | Default | Description |\n|---|---|---|\n";
        for (const auto& o : opts)
          s += "| `" + md(o.name) + "` | " + (o.defaultValue.empty() ? std::string() : "`" + md(o.defaultValue) + "`") +
               " | " + md(o.description) + " |\n";
        s += "\n";
      }
      if (!c.notes.empty()) {
        s += "Notes:\n\n";
        for (const auto& x : c.notes) s += "- " + x + "\n";
        s += "\n";
      }
      if (!c.limitations.empty()) {
        s += "Limitations:\n\n";
        for (const auto& x : c.limitations) s += "- " + x + "\n";
        s += "\n";
      }
    }
  }
  if (section.empty() || section == "backends") {
    s += "## Backends\n\n";
    for (const auto& n : backend::Registry::Global().Names()) {
      if (n.rfind("test-", 0) == 0 || !backend::Registry::Global().AliasOf(n).empty()) continue;
      const auto c = backend::Registry::Global().Create(n)->GetCapabilities();
      s += "### " + n + "\n\n" + c.description + "\n\n";
      for (const auto& a : backend::Registry::Global().Names())
        if (backend::Registry::Global().AliasOf(a) == n) s += "Also registered as `" + a + "` (former name).\n\n";
      s += "| Capability | |\n|---|---|\n";
      s += "| Element kinds | " + join(c.kinds, [](data::ElementKind k) { return std::string(data::ToString(k)); }) + " |\n";
      s += "| Style properties | " + join(c.styleProperties, [](const std::string& p) { return "`" + p + "`"; }) + " |\n";
      s += std::string("| Split layouts | ") + (c.splits ? "yes" : "no") + " |\n";
      s += std::string("| Tab layouts | ") + (c.tabs ? "yes" : "no (first tab shown)") + " |\n\n";
      if (!c.options.empty()) {
        s += "| Option | Description |\n|---|---|\n";
        for (const auto& [o, d] : c.options) s += "| `" + md(o) + "` | " + md(d) + " |\n";
        s += "\n";
      }
    }
  }
  return s;
}

// ---------------------------------------------------------------------------
// Completion
// ---------------------------------------------------------------------------
std::vector<std::string> Interpreter::Complete(std::string_view lineIn) const {
  std::string line(lineIn);
  std::vector<std::string> words;
  try {
    words = SplitWords(line);
  } catch (const ParseError&) {
    return {};  // inside an unterminated string
  }
  const bool fresh = line.empty() || std::isspace(static_cast<unsigned char>(line.back()));
  std::string prefix;
  if (!fresh && !words.empty()) {
    prefix = words.back();
    words.pop_back();
  }
  std::set<std::string> out;
  auto offer = [&](const std::string& c) {
    if (c.rfind(prefix, 0) == 0) out.insert(c);
  };

  // Command names
  if (words.empty()) {
    for (const auto& f : Forms()) offer(f.words[0]);
    offer("exit");
    return {out.begin(), out.end()};
  }

  // Find the forms whose fixed words match the line
  std::vector<const CommandForm*> forms;
  for (const auto& f : Forms()) {
    bool match = f.words[0] == words[0] || (words[0] == "exit" && f.words[0] == "quit");
    for (std::size_t k = 1; match && k < f.words.size(); ++k) match = k < words.size() && words[k] == f.words[k];
    if (match) forms.push_back(&f);
  }
  // Subcommand position: offer the second fixed words
  if (words.size() == 1) {
    bool sub = false;
    for (const auto& f : Forms())
      if (f.words[0] == words[0] && f.words.size() > 1) {
        offer(f.words[1]);
        sub = true;
      }
    if (sub) return {out.begin(), out.end()};
  }

  auto& session = const_cast<session::Session&>(m_session);
  auto layerPaths = [&] {
    for (const auto& sc : session.SceneNames())
      if (auto scene = session.FindScene(sc))
        for (const auto& [l, t] : scene->layers.Types()) offer(sc + "/" + l);
  };
  auto layerKind = [&](const std::string& path) -> std::optional<ElementKind> {
    const auto slash = path.find('/');
    if (slash == std::string::npos) return std::nullopt;
    auto scene = session.FindScene(path.substr(0, slash));
    if (!scene) return std::nullopt;
    const auto types = scene->layers.Types();
    auto it = types.find(path.substr(slash + 1));
    if (it == types.end()) return std::nullopt;
    return data::KindOf(it->second);
  };
  auto attributes = [&](const std::string& path) {
    std::set<std::string> keys;
    const auto slash = path.find('/');
    if (slash == std::string::npos) return keys;
    auto scene = session.FindScene(path.substr(0, slash));
    auto kind = layerKind(path);
    if (!scene || !kind) return keys;
    WithKind(*kind, [&]<typename X>() {
      const auto* l = scene->layers.Find<X>(path.substr(slash + 1));
      if (!l || !l->series || session.Data().Size<X>(*l->series) == 0) return;
      if (auto e = session.Data().Entry<X>(*l->series, 0))
        for (const auto& el : *e)
          for (const auto& [k, v] : el.meta.All())
            if (k.rfind("unit:", 0) != 0) keys.insert(k);
    });
    return keys;
  };
  auto keyValues = [&](const std::vector<std::string>& names) {
    for (const auto& n : names) offer(n + "=");
  };

  // Command-specific positions the generic table cannot express
  if (words[0] == "camera" && words.size() == 3) {
    if (words[2] == "mode")
      for (const char* m : {"animated", "fixed", "free"}) offer(m);
    if (words[2] == "share")
      for (const auto& sc : m_session.SceneNames()) offer(sc);
    return {out.begin(), out.end()};
  }
  if (words[0] == "style" && words.size() == 3 && words[2] == "preset") {
    std::optional<ElementKind> k;
    if (auto slash = words[1].find('/'); slash != std::string::npos)
      if (auto scene = m_session.FindScene(words[1].substr(0, slash))) {
        const auto types = scene->layers.Types();
        if (auto it = types.find(words[1].substr(slash + 1)); it != types.end()) k = data::KindOf(it->second);
      }
    if (!k || *k == ElementKind::Point) offer("hits-by-energy");
    if (!k || *k == ElementKind::Line) offer("tracks-by-charge");
    if (!k || *k == ElementKind::Surface) offer("detector-outline");
    for (const char* p : {"color-by", "ghosted", "size-by"}) offer(p);
    return {out.begin(), out.end()};
  }
  if (words[0] == "backend" && words.size() >= 3 && words[1] == "init") {
    if (auto b = backend::Registry::Global().Create(words[2]))
      for (const auto& [n, d] : b->GetCapabilities().options) offer(n + "=");
    return {out.begin(), out.end()};
  }

  for (const auto* f : forms) {
    const std::size_t argIndex = words.size() - f->words.size();
    if (words.size() < f->words.size()) continue;
    // rest-of-line kinds and repeated kinds take every later position
    ArgKind kind = ArgKind::Text;
    bool found = false;
    for (std::size_t k = 0; k < f->args.size(); ++k) {
      const ArgKind a = f->args[k];
      const bool rest = a == ArgKind::Filter || a == ArgKind::Style || a == ArgKind::Environment ||
                        a == ArgKind::Layout || a == ArgKind::AdapterOption || a == ArgKind::BackendOption ||
                        a == ArgKind::Text;
      if (k == argIndex || (rest && argIndex >= k)) {
        kind = a;
        found = true;
        break;
      }
    }
    // repeated scene arguments (scene rm a b, open id a b)
    if (!found && !f->args.empty() && (f->args.back() == ArgKind::Scene)) {
      kind = ArgKind::Scene;
      found = true;
    }
    if (!found) continue;
    const std::string& lastWord = words.back();
    switch (kind) {
      case ArgKind::Keyword:
        if (f->words[0] == "layer") {
          if (lastWord == "as" || lastWord == "entry") break;
          for (const auto& k : f->keywords) offer(k);
        } else if ((f->words[0] == "camera" || f->words[0] == "entry") && argIndex == 1) {
          for (const auto& k : f->keywords) offer(k);
        } else if (f->words[0] == "style" && argIndex == 1) {
          offer("preset");
          offer("when");
        }
        break;
      case ArgKind::Scene:
        for (const auto& sc : session.SceneNames()) offer(sc);
        break;
      case ArgKind::LayerPath: layerPaths(); break;
      case ArgKind::Series:
        for (const auto& info : session.Data().List()) offer(info.name);
        break;
      case ArgKind::Adapter:
        for (const auto& n : adapters::Registry::Global().Names()) offer(n);
        break;
      case ArgKind::AdapterOption:
        if (argIndex >= 2)
          if (auto a = adapters::Registry::Global().Create(words[1])) {
            std::vector<std::string> names;
            for (const auto& o : a->GetCapabilities().options) names.push_back(o.name);
            keyValues(names);
          }
        break;
      case ArgKind::BackendType:
        for (const auto& n : backend::Registry::Global().Names())
          if (n.rfind("test-", 0) != 0) offer(n);
        break;
      case ArgKind::BackendId:
        for (const auto& id : session.BackendIds()) offer(id);
        break;
      case ArgKind::BackendOption:
        if (auto b = backend::Registry::Global().Create(words[2])) {
          std::vector<std::string> names;
          for (const auto& [n, d] : b->GetCapabilities().options) names.push_back(n);
          keyValues(names);
        }
        break;
      case ArgKind::Filter: {
        for (const auto& k : attributes(words[1])) offer(k);
        if (argIndex > 1)
          for (const char* w : {"and", "or", "not", "in"}) offer(w);
        for (const char* fn : {"all()", "between(", "exists(", "first(", "index(", "inside(", "none()", "outside(", "within("})
          offer(fn);
        break;
      }
      case ArgKind::Style: {
        // after 'when' and before 'then': a filter; otherwise style keys
        bool inWhen = false;
        for (std::size_t k = 2; k < words.size(); ++k) {
          if (words[k] == "when") inWhen = true;
          if (words[k] == "then") inWhen = false;
        }
        if (words.size() >= 3 && words[2] == "preset") break;
        if (inWhen) {
          for (const auto& k : attributes(words[1])) offer(k);
          offer("then");
          for (const char* w : {"and", "or", "not"}) offer(w);
        } else if (auto k = layerKind(words[1])) {
          keyValues(StyleKeys(KindName(*k)));
          if (words.size() == 2) {
            offer("preset");
            offer("when");
          }
        }
        break;
      }
      case ArgKind::Environment:
        if (words.size() == 2) offer("preset");
        if (words.size() == 3 && words[2] == "preset") {
          offer("default");
          offer("publication");
          break;
        }
        keyValues(StyleKeys("environment"));
        break;
      case ArgKind::Layout:
        if (words.size() == 1) offer("show");
        for (const auto& sc : session.SceneNames()) offer(sc);
        for (const char* fn : {"split(", "tabs(", "vsplit("}) offer(fn);
        break;
      case ArgKind::Topic:
        if (argIndex == 0) {
          for (const auto& g : Forms()) offer(g.words[0]);
          for (const char* t : {"adapter", "adapters", "backend", "backends", "filters", "layouts", "styles"}) offer(t);
        } else if (argIndex == 1 && words[1] == "adapter") {
          for (const auto& n : adapters::Registry::Global().Names()) offer(n);
        } else if (argIndex == 1 && words[1] == "backend") {
          for (const auto& n : backend::Registry::Global().Names())
            if (n.rfind("test-", 0) != 0) offer(n);
        }
        break;
      case ArgKind::Name:
        if (f->words[0] == "help" && argIndex == 1) {
          if (words[1] == "adapter")
            for (const auto& n : adapters::Registry::Global().Names()) offer(n);
          if (words[1] == "backend")
            for (const auto& n : backend::Registry::Global().Names())
              if (n.rfind("test-", 0) != 0) offer(n);
        }
        break;
      case ArgKind::File: {
        namespace fs = std::filesystem;
        const std::string base = m_base.empty() ? std::string(".") : m_base;
        const auto slash = prefix.rfind('/');
        const std::string dirPart = slash == std::string::npos ? std::string() : prefix.substr(0, slash + 1);
        const fs::path dir = fs::path(dirPart.empty() ? base : (fs::path(dirPart).is_absolute() ? dirPart : (fs::path(base) / dirPart).string()));
        std::error_code ec;
        for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
          const std::string name = it->path().filename().string();
          if (!name.empty() && name[0] == '.') continue;
          offer(dirPart + name + (it->is_directory(ec) ? "/" : ""));
        }
        break;
      }
      case ArgKind::Number:
      case ArgKind::Text: break;
    }
  }
  return {out.begin(), out.end()};
}

}  // namespace fccvis::api
