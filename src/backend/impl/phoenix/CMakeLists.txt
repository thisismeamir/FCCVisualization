# B5 Phoenix backend: event-data JSON, OBJ geometry, optional HTTP host. No dependencies.
fccvis_add_module(backend_phoenix KIND backend_impl
  SOURCES HttpServer.cc PhoenixBackend.cc
  INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS backend stdlib)
