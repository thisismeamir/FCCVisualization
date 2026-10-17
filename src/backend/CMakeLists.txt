# Backend contract (Backend.h, BackendRegistry.h, RecordingBackend.h) and the
# helpers shared by backends (RevisionTracker, Executor, Drawing).
fccvis_add_module(backend
  SOURCES Drawing.cc FileBackend.cc
  DEPENDS changes scene data)

add_subdirectory(impl)
