# ---------------------------------------------------------------------------
# Core object library (no ROOT/TEve dependency)
# ---------------------------------------------------------------------------
add_subdirectory(api)
add_subdirectory(core)

add_subdirectory(geometry)
add_subdirectory(filter)
add_subdirectory(scene)
add_subdirectory(visualization)
add_subdirectory(session)
add_library(fccvis_core OBJECT
    $<TARGET_OBJECTS:fccvis_api>
    $<TARGET_OBJECTS:fccvis_core_impl>
    $<TARGET_OBJECTS:fccvis_geometry>
    $<TARGET_OBJECTS:fccvis_scene>
    $<TARGET_OBJECTS:fccvis_filter>
    $<TARGET_OBJECTS:fccvis_visualization>
    $<TARGET_OBJECTS:fccvis_session>
)

target_include_directories(fccvis_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(fccvis_core PUBLIC
    PandoraPFA::PandoraSDK
)

set_target_properties(fccvis_core PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# ---------------------------------------------------------------------------
# ROOT/TEve backend object library (always required — headless monitoring
# still renders via TEve; FCCVIS_ENABLE_GUI only gates interactive windows)
# ---------------------------------------------------------------------------
add_subdirectory(backend/root)

if(FCCVIS_ENABLE_GUI)
    add_subdirectory(gui)
endif()

# ---------------------------------------------------------------------------
# Assemble final library target(s)
# ---------------------------------------------------------------------------
set(_fccvis_all_objects
    $<TARGET_OBJECTS:fccvis_api>
    $<TARGET_OBJECTS:fccvis_core_impl>
    $<TARGET_OBJECTS:fccvis_geometry>
    $<TARGET_OBJECTS:fccvis_filter>
    $<TARGET_OBJECTS:fccvis_scene>
    $<TARGET_OBJECTS:fccvis_visualization>
    $<TARGET_OBJECTS:fccvis_core>
    $<TARGET_OBJECTS:fccvis_session>
    $<TARGET_OBJECTS:fccvis_backend_root>
)
if(FCCVIS_ENABLE_GUI)
    list(APPEND _fccvis_all_objects $<TARGET_OBJECTS:fccvis_gui>)
endif()

if(FCCVIS_BUILD_SHARED)
    add_library(FCCVisualization SHARED ${_fccvis_all_objects})
    add_library(FCCVisualization::FCCVisualization ALIAS FCCVisualization)
    set_target_properties(FCCVisualization PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
endif()

if(FCCVIS_BUILD_STATIC)
    add_library(FCCVisualization_static STATIC ${_fccvis_all_objects})
    add_library(FCCVisualization::FCCVisualization_static ALIAS FCCVisualization_static)
    set_target_properties(FCCVisualization_static PROPERTIES OUTPUT_NAME FCCVisualization)
endif()

if(TARGET FCCVisualization::FCCVisualization)
    set(FCCVIS_LINK_LIB FCCVisualization::FCCVisualization)
else()
    set(FCCVIS_LINK_LIB FCCVisualization::FCCVisualization_static)
endif()

add_subdirectory(cli)

foreach(_tgt FCCVisualization FCCVisualization_static)
    if(TARGET ${_tgt})
        target_include_directories(${_tgt} PUBLIC
            $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        )
        target_link_libraries(${_tgt} PUBLIC
            PandoraPFA::PandoraSDK
            ROOT::Core ROOT::RIO ROOT::Eve ROOT::Geom ROOT::RGL ROOT::EG
        )
        set_target_properties(${_tgt} PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED ON
        )
        if(FCCVIS_ENABLE_GUI)
            target_link_libraries(${_tgt} PUBLIC ROOT::Gui)
            target_compile_definitions(${_tgt} PUBLIC FCCVIS_ENABLE_GUI)
        endif()
    endif()
endforeach()
