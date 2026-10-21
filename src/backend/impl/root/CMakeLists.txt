# B1 ROOT backends: classic TEve ("root", needs an X display at run time) and,
# when ROOT has the web viewer, REve ("reve", served over HTTP to a browser).
# Both share one ROOT thread (RootRuntime).
set(_root_sources RootRuntime.cc RootBackend.cc)
set(_root_libs ROOT::Core ROOT::Gui ROOT::Eve ROOT::RGL ROOT::Ged ROOT::Geom ROOT::Graf3d)
if(FCCVIS_ENABLE_REVE)
  list(APPEND _root_sources RootWebBackend.cc)
  list(APPEND _root_libs ROOT::ROOTEve ROOT::ROOTWebDisplay ROOT::RHTTP)
endif()
fccvis_add_module(backend_root KIND backend_impl
  SOURCES ${_root_sources}
  INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS backend
  LIBS ${_root_libs})
