find_package(Catch2 3 REQUIRED)
include(CTest)
include(Catch)

set(TEST_RESOURCES_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/resources)

set(TEST_RESOURCES_OUTPUT_DIR
    ${CMAKE_CURRENT_BINARY_DIR}/resources)

add_custom_target(copy_test_resources ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${TEST_RESOURCES_DIR}
            ${TEST_RESOURCES_OUTPUT_DIR}
    COMMENT "Copying test resources"
)

# ---------------------------------------------------------------------------
# Geometry
# ---------------------------------------------------------------------------

add_executable(fccvis_geometry_tests
    geometry/test_abstracts.cc
    geometry/test_shapes.cc
    geometry/test_transformations.cc
    geometry/test_placement.cc
    geometry/test_adapters_basics.cc
    geometry/test_adapters_tgeo.cc
    geometry/test_detector_node.cc
    geometry/test_detector_tree.cc
    geometry/test_adapters_pipeline.cc
)

add_dependencies(
    fccvis_geometry_tests
    copy_test_resources
)

target_compile_definitions(
    fccvis_geometry_tests
    PRIVATE
        TEST_RESOURCE_DIR="${TEST_RESOURCES_OUTPUT_DIR}"
)

target_link_libraries(
    fccvis_geometry_tests
    PRIVATE
        fccvis_geometry
        Catch2::Catch2WithMain
)

catch_discover_tests(fccvis_geometry_tests)
