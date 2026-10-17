# A3 CSV adapter: delimiter-separated text -> point/line series. No dependencies.
fccvis_add_module(adapter_csv KIND adapter
  SOURCES CsvAdapter.cc
  INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS adapters data)
