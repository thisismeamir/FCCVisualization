# B4 CED backend: speaks the CED (glced) TCP protocol itself; no dependency (POSIX sockets).
fccvis_add_module(backend_ced KIND backend_impl
  SOURCES CedBackend.cc
  INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS backend)
