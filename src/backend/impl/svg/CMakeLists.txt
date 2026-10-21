# B2 SVG export backend. No dependencies.
fccvis_add_module(backend_svg KIND backend_impl
  SOURCES Svg.cc SvgBackend.cc
  INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS backend)
