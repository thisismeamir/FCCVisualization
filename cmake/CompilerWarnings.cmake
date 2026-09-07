add_library(fccvis_compiler_warnings INTERFACE)
add_library(FCCVisualization::CompilerWarnings ALIAS fccvis_compiler_warnings)

target_compile_options(fccvis_compiler_warnings INTERFACE
    -Wall
    -Wextra
    -Werror
    -pedantic
    -Wno-long-long
    -Wno-sign-compare
    -Wshadow
    -fno-strict-aliasing
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(fccvis_compiler_warnings INTERFACE -Wno-dtor-name)
endif()
