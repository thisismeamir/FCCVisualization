# A4 Phoenix JSON adapter: Phoenix event-data files -> point and line series. No dependencies.
fccvis_add_module(adapter_phoenix KIND adapter
  SOURCES Json.cc PhoenixAdapter.cc
  DEPENDS adapters data)
