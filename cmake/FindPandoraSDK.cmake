# FindPandoraSDK.cmake
#
# Locates the PandoraSDK installation.
#
# Respects:
#   PandoraSDK_DIR       - CMake config-file search hint
#   PANDORA_DIR / PandoraSDK_ROOT - root install prefix hint
#   ENV{PANDORA_DIR}
#
# Defines:
#   PandoraSDK_FOUND
#   PandoraSDK_INCLUDE_DIR
#   PandoraSDK_LIBRARY
#   PandoraSDK::PandoraSDK  (imported target)

set(_pandora_hints
    ${PandoraSDK_ROOT}
    ${PANDORA_DIR}
    $ENV{PANDORA_DIR}
)

find_path(PandoraSDK_INCLUDE_DIR
    NAMES Pandora/Pandora.h
    HINTS ${_pandora_hints}
    PATH_SUFFIXES include
)

find_library(PandoraSDK_LIBRARY
    NAMES PandoraSDK
    HINTS ${_pandora_hints}
    PATH_SUFFIXES lib lib64
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PandoraSDK
    REQUIRED_VARS PandoraSDK_LIBRARY PandoraSDK_INCLUDE_DIR
)

if(PandoraSDK_FOUND AND NOT TARGET PandoraPFA::PandoraSDK)
    add_library(PandoraPFA::PandoraSDK UNKNOWN IMPORTED)
    set_target_properties(PandoraPFA::PandoraSDK PROPERTIES
        IMPORTED_LOCATION "${PandoraSDK_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${PandoraSDK_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(PandoraSDK_INCLUDE_DIR PandoraSDK_LIBRARY)
