# FindPodio.cmake
#
# Locates a podio installation.
#
# Respects:
#   Podio_ROOT
#   PODIO_DIR
#   ENV{PODIO_DIR}
#
# Defines:
#   Podio_FOUND
#   Podio_INCLUDE_DIR
#   Podio_LIBRARY
#   Podio::Podio       (imported target)

set(_podio_hints
    ${Podio_ROOT}
    ${PODIO_DIR}
    $ENV{PODIO_DIR}
)

find_path(Podio_INCLUDE_DIR
    NAMES podio/CollectionBase.h
    HINTS ${_podio_hints}
    PATH_SUFFIXES include
)

find_library(Podio_LIBRARY
    NAMES podio
    HINTS ${_podio_hints}
    PATH_SUFFIXES lib lib64
)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(Podio
    REQUIRED_VARS
        Podio_LIBRARY
        Podio_INCLUDE_DIR
)

if(Podio_FOUND AND NOT TARGET Podio::Podio)
    add_library(Podio::Podio UNKNOWN IMPORTED)

    set_target_properties(Podio::Podio PROPERTIES
        IMPORTED_LOCATION
            "${Podio_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES
            "${Podio_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(
    Podio_INCLUDE_DIR
    Podio_LIBRARY
)
