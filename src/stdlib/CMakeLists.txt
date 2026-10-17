# Standard library: shapes, filters, style presets (SPEC_STDLIB.md).
fccvis_add_module(stdlib
  SOURCES Shapes.cc Filters.cc ShapeFilters.cc DetectorFilters.cc Styles.cc MeshExport.cc
  DEPENDS geometry filter style data)
