add_library(fccvis_gui OBJECT
    GuiApplication.cc
)

target_include_directories(fccvis_gui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fccvis_gui PRIVATE
    FCCVisualization::CompilerWarnings
    ROOT::Core ROOT::Eve ROOT::Gui
    fccvis_session
)

set_target_properties(fccvis_gui PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)
