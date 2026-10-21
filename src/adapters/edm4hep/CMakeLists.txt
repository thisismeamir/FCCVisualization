# A2 EDM4hep adapter: podio frame files -> point and line series. Requires podio (IO) and EDM4hep.
fccvis_add_module(adapter_edm4hep KIND adapter
  SOURCES Edm4hepAdapter.cc
  DEPENDS adapters data
  LIBS EDM4HEP::edm4hep podio::podio podio::podioIO podio::podioRootIO)
