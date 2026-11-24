# B3 histogram maker backend. No dependencies.
fccvis_add_module(backend_histogram KIND backend_impl
  SOURCES Histogram.cc HistogramBackend.cc
  INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS backend)
