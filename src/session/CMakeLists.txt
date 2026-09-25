add_library(fccvis_session OBJECT
  Session.cc
)

target_include_directories(fccvis_session PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)


find_package(podio CONFIG REQUIRED)
find_package(EDM4HEP CONFIG REQUIRED)
target_link_libraries(fccvis_session PUBLIC
    FCCVisualization::CompilerWarnings
    fccvis_geometry
    fccvis_scene
    podio::podio
    podio::podioRootIO
    EDM4HEP::edm4hep
)

set_target_properties(fccvis_session PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)
