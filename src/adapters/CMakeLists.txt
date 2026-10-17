# Adapter contract (Adapter.h, AdapterRegistry.h): data only.
fccvis_add_module(adapters SOURCES Adapter.cc DEPENDS data)

# Adapter implementations, each behind FCCVIS_ENABLE_<NAME>
if(FCCVIS_ENABLE_CSV)
  add_subdirectory(csv)
endif()
if(FCCVIS_ENABLE_DD4HEP)
  add_subdirectory(detector)
endif()
if(FCCVIS_ENABLE_EDM4HEP)
  add_subdirectory(edm4hep)
endif()
if(FCCVIS_ENABLE_PHOENIX_JSON)
  add_subdirectory(phoenix)
endif()
