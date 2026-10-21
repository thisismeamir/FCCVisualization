# B0 raster backend (PNG, no dependencies). Registered as "raster" and "fv".
fccvis_add_module(backend_raster KIND backend_impl
  SOURCES RasterBackend.cc Canvas.cc Png.cc
  INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS backend)
