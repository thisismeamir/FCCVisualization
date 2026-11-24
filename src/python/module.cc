// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// Python binding (T5.8): the `fccvis` module.
//
// A thin layer over the C++ API: it constructs changes (through api::Script
// and the expression parsers of the shell) and calls the session; it adds no
// behaviour of its own (SPEC_INTERFACE "Bindings").
#include "AdapterRegistry.h"
#include "BackendRegistry.h"
#include "CameraChanges.h"
#include "SceneChanges.h"
#include "Session.h"
#include "fccvis/FccvisConfig.h"
#include "fccvis/api/Builders.h"
#include "fccvis/api/Expression.h"
#include "fccvis/api/Interpreter.h"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace py = pybind11;
using namespace fccvis;

namespace {

/** Error raised for a failed command or change. */
struct Error : std::runtime_error {
  using std::runtime_error::runtime_error;
};

std::map<std::string, std::string> Options(const py::kwargs& kwargs) {
  std::map<std::string, std::string> out;
  for (const auto& [k, v] : kwargs) {
    const auto key = py::str(k).cast<std::string>();
    if (py::isinstance<py::bool_>(v)) out[key] = v.cast<bool>() ? "true" : "false";
    else out[key] = py::str(v).cast<std::string>();
  }
  return out;
}

/** A session with the shell interpreter attached (for execute()). */
class PySession {
 public:
  explicit PySession(const std::string& name) : m_session(name), m_shell(m_session) {}

  session::Session& Get() { return m_session; }
  api::Interpreter& Shell() { return m_shell; }

 private:
  session::Session m_session;
  api::Interpreter m_shell;
};

/** Builder of changes for one scene; the layer methods select the layer that where()/style() apply to. */
class PyScene {
 public:
  explicit PyScene(std::string name) : m_name(std::move(name)) {}

  PyScene& Create() {
    Append(api::Scene(m_name).Create());
    return *this;
  }
  PyScene& Remove() {
    Append(api::Scene(m_name).Remove());
    return *this;
  }
  PyScene& Direction(double x, double y, double z) {
    Append(api::Scene(m_name).Direction({x, y, z}));
    return *this;
  }
  PyScene& Position(double x, double y, double z) {
    Append(api::Scene(m_name).Position({x, y, z}));
    return *this;
  }
  PyScene& Zoom(double m) {
    Append(api::Scene(m_name).Zoom(m));
    return *this;
  }
  PyScene& Environment(const std::string& text) {
    m_script.Add(changes::SetEnvironment{m_name, api::ParseEnvironment(text)});
    return *this;
  }
  PyScene& Layer(const std::string& kind, const std::string& layer, const std::string& series, std::size_t entry) {
    if (kind == "point") m_script.Add(changes::AddLayer<data::PointElement>{m_name, layer, {series, ""}, entry});
    else if (kind == "line") m_script.Add(changes::AddLayer<data::LineElement>{m_name, layer, {series, ""}, entry});
    else if (kind == "surface") m_script.Add(changes::AddLayer<data::SurfaceElement>{m_name, layer, {series, ""}, entry});
    else throw py::value_error("kind must be 'point', 'line' or 'surface', got '" + kind + "'");
    m_layer = layer;
    m_kind = kind;
    return *this;
  }
  PyScene& Select(const std::string& kind, const std::string& layer) {
    if (kind != "point" && kind != "line" && kind != "surface")
      throw py::value_error("kind must be 'point', 'line' or 'surface', got '" + kind + "'");
    m_layer = layer;
    m_kind = kind;
    return *this;
  }
  PyScene& Where(const std::string& expression) {
    RequireLayer("where");
    const stdlib::Selector sel = api::ParseFilter(expression);
    if (m_kind == "point") AddFilter<data::PointElement>(sel);
    else if (m_kind == "line") AddFilter<data::LineElement>(sel);
    else AddFilter<data::SurfaceElement>(sel);
    return *this;
  }
  PyScene& Style(const std::string& assignments, const std::string& when) {
    RequireLayer("style");
    const stdlib::Selector sel = when.empty() ? stdlib::All() : api::ParseFilter(when);
    if (m_kind == "point")
      m_script.Add(changes::AddStyleRule<data::PointElement>{m_name, m_layer, sel, api::ParsePointStyle(assignments)});
    else if (m_kind == "line")
      m_script.Add(changes::AddStyleRule<data::LineElement>{m_name, m_layer, sel, api::ParseLineStyle(assignments)});
    else
      m_script.Add(changes::AddStyleRule<data::SurfaceElement>{m_name, m_layer, sel, api::ParseSurfaceStyle(assignments)});
    return *this;
  }
  PyScene& Entry(std::size_t entry) {
    RequireLayer("entry");
    if (m_kind == "point") m_script.Add(changes::SetEntry<data::PointElement>{m_name, m_layer, entry});
    else if (m_kind == "line") m_script.Add(changes::SetEntry<data::LineElement>{m_name, m_layer, entry});
    else m_script.Add(changes::SetEntry<data::SurfaceElement>{m_name, m_layer, entry});
    return *this;
  }

  [[nodiscard]] const api::Script& Script() const { return m_script; }

 private:
  void Append(const api::Script& s) { m_script.Append(s); }
  void RequireLayer(const char* what) const {
    if (m_layer.empty()) throw py::value_error(std::string(what) + "() needs a layer: call points(), lines(), surfaces() or layer() first");
  }
  template <typename X>
  void AddFilter(const stdlib::Selector& sel) {
    m_script.Add(changes::SetLayerFilter<X>{m_name, m_layer, std::make_shared<filter::Filter<X>>(sel.For<X>())});
  }

  std::string m_name, m_layer, m_kind;
  api::Script m_script;
};

changes::Result Check(changes::Result r) {
  if (!r.ok) throw Error(r.messages.empty() ? std::string("failed") : r.messages.front());
  return r;
}

}  // namespace

PYBIND11_MODULE(fccvis, m) {
  m.doc() = "fccvis: event and detector displays (Python binding of the C++ API)";
  m.attr("__version__") = FCCVIS_VERSION;
  py::register_exception<Error>(m, "Error", PyExc_RuntimeError);
  py::register_exception_translator([](std::exception_ptr p) {
    try {
      if (p) std::rethrow_exception(p);
    } catch (const api::ParseError& e) {  // malformed filter, style or layout text
      PyErr_SetString(PyExc_ValueError, e.what());
    }
  });

  py::class_<changes::Result>(m, "Result", "Outcome of resolving changes.")
      .def_readonly("ok", &changes::Result::ok)
      .def_readonly("applied", &changes::Result::applied)
      .def_readonly("messages", &changes::Result::messages)
      .def("__bool__", [](const changes::Result& r) { return r.ok; })
      .def("__repr__", [](const changes::Result& r) {
        return "<Result ok=" + std::string(r.ok ? "True" : "False") + " applied=" + std::to_string(r.applied) + ">";
      });

  py::class_<adapters::LoadReport>(m, "LoadReport", "What an adapter registered and skipped.")
      .def_readonly("source", &adapters::LoadReport::source)
      .def_property_readonly("ok", &adapters::LoadReport::Ok)
      .def_property_readonly("series", [](const adapters::LoadReport& r) {
        std::vector<std::string> out;
        for (const auto& s : r.registered) out.push_back(s.ref.name);
        return out;
      })
      .def_property_readonly("skipped", [](const adapters::LoadReport& r) {
        std::vector<std::pair<std::string, std::string>> out;
        for (const auto& s : r.skipped) out.emplace_back(s.name, s.reason);
        return out;
      })
      .def("__str__", [](const adapters::LoadReport& r) { return r.ToString(); });

  py::class_<api::Script>(m, "Script", "An ordered list of changes, run as one batch.")
      .def(py::init<>())
      .def("__len__", &api::Script::Size)
      .def("describe", &api::Script::Describe, "One line per change.")
      .def("__iadd__", [](api::Script& s, const api::Script& o) -> api::Script& { return s.Append(o); })
      .def("__iadd__", [](api::Script& s, const PyScene& o) -> api::Script& { return s.Append(o.Script()); });

  py::class_<PyScene>(m, "Scene", "Builds the changes of one scene; methods chain.")
      .def(py::init<std::string>(), py::arg("name"))
      .def("create", &PyScene::Create, "Create the scene (with its own camera).")
      .def("remove", &PyScene::Remove, "Remove the scene.")
      .def("direction", &PyScene::Direction, py::arg("x"), py::arg("y"), py::arg("z"), "Viewing direction.")
      .def("position", &PyScene::Position, py::arg("x"), py::arg("y"), py::arg("z"), "Camera pan.")
      .def("zoom", &PyScene::Zoom, py::arg("magnification"))
      .def("environment", &PyScene::Environment, py::arg("assignments"), "For example 'axes=true background=#ffffff'.")
      .def("points", [](PyScene& s, const std::string& l, const std::string& series, std::size_t e) -> PyScene& {
             return s.Layer("point", l, series, e); }, py::arg("layer"), py::arg("series"), py::arg("entry") = 0,
           "Add a point layer showing a series.")
      .def("lines", [](PyScene& s, const std::string& l, const std::string& series, std::size_t e) -> PyScene& {
             return s.Layer("line", l, series, e); }, py::arg("layer"), py::arg("series"), py::arg("entry") = 0,
           "Add a line layer.")
      .def("surfaces", [](PyScene& s, const std::string& l, const std::string& series, std::size_t e) -> PyScene& {
             return s.Layer("surface", l, series, e); }, py::arg("layer"), py::arg("series"), py::arg("entry") = 0,
           "Add a surface layer.")
      .def("layer", &PyScene::Select, py::arg("kind"), py::arg("layer"),
           "Select an existing layer ('point', 'line' or 'surface') for where()/style()/entry().")
      .def("where", &PyScene::Where, py::arg("expression"), "Filter of the current layer (shell filter syntax).")
      .def("style", &PyScene::Style, py::arg("assignments"), py::arg("when") = std::string(),
           "Style rule of the current layer, for example style('color=red size=5', when='energy > 1').")
      .def("entry", &PyScene::Entry, py::arg("entry"), "Entry (event) of the current layer.")
      .def_property_readonly("script", &PyScene::Script, "The changes built so far.");

  m.def("layout", [](const std::string& expression) {
          api::Script s;
          s.Append(api::Layout(api::ParseLayout(expression)));
          return s;
        }, py::arg("expression"), "Script setting the layout, for example layout('split(xy, rz)').");

  m.def("adapters", [] { return adapters::Registry::Global().Names(); }, "Registered adapter names.");
  m.def("backends", [] { return backend::Registry::Global().Names(); }, "Registered backend types.");

  py::class_<PySession>(m, "Session", "A visualization session (data, scenes, layout, backends).")
      .def(py::init<std::string>(), py::arg("name") = std::string("python"))
      .def("load", [](PySession& s, const std::string& adapter, const std::string& path, const py::kwargs& kw) {
             auto a = adapters::Registry::Global().Create(adapter);
             if (!a) throw py::value_error("unknown adapter '" + adapter + "'");
             auto report = a->Open(s.Get().Data(), path, Options(kw));
             s.Get().DataChanged();
             return report;
           }, py::arg("adapter"), py::arg("path"), "Open a source; options as keyword arguments.")
      .def("series", [](PySession& s) {
             std::vector<py::dict> out;
             for (const auto& info : s.Get().Data().List()) {
               py::dict d;
               d["name"] = info.name;
               d["source"] = info.source;
               const auto kind = data::KindOf(info.type);
               d["kind"] = kind ? std::string(data::ToString(*kind)) : std::string("other");
               d["entries"] = info.entries;
               d["description"] = info.description;
               out.push_back(d);
             }
             return out;
           }, "Registered series: name, source, kind, entries, description.")
      .def("resolve", [](PySession& s, const api::Script& script) { return Check(script.Run(s.Get())); },
           py::arg("script"), "Run a script as one batch; raises fccvis.Error if a change fails.")
      .def("resolve", [](PySession& s, const PyScene& scene) { return Check(scene.Script().Run(s.Get())); },
           py::arg("scene"))
      .def("execute", [](PySession& s, const std::string& command) {
             const auto reply = s.Shell().Execute(command);
             if (!reply.ok) throw Error(reply.errors.empty() ? std::string("failed") : reply.errors.front());
             return reply.lines;
           }, py::arg("command"), "Run one shell command; returns its output lines.")
      .def("scene_names", [](PySession& s) { return s.Get().SceneNames(); })
      .def("init_backend", [](PySession& s, const std::string& type, const std::string& id, const py::kwargs& kw) {
             Check(s.Get().InitBackend(type, id, Options(kw)));
           }, py::arg("type"), py::arg("id"), "Attach a backend; options as keyword arguments.")
      .def("open", [](PySession& s, const std::string& id, const std::string& scene) { Check(s.Get().Open(id, scene)); },
           py::arg("backend"), py::arg("scene"))
      .def("close", [](PySession& s, const std::string& id, const std::string& scene) { Check(s.Get().Close(id, scene)); },
           py::arg("backend"), py::arg("scene"))
      .def("detach", [](PySession& s, const std::string& id) { Check(s.Get().DetachBackend(id)); }, py::arg("backend"))
      .def("backend_ids", [](PySession& s) { return s.Get().BackendIds(); })
      .def("flush", [](PySession& s, const std::string& id) {
             py::gil_scoped_release release;
             return s.Get().Flush(id);
           }, py::arg("backend") = std::string(), "Wait until backends finished drawing.")
      .def("undo", [](PySession& s) { return s.Get().Undo(); }, "Undo the last step (see Result.ok).")
      .def("redo", [](PySession& s) { return s.Get().Redo(); }, "Redo the last undone step.");

}
