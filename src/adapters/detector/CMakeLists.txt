# A1 detector adapter: DD4hep/TGeo -> geometry. Requires DD4hep and ROOT Geom.
fccvis_add_module(adapter_detector KIND adapter
  SOURCES TGeoShapeAdapter.cc DetectorNode.cc DetectorAdapter.cc
  DEPENDS adapters data
  LIBS DD4hep::DDCore ROOT::Core ROOT::Geom)
