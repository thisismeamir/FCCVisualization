function(fccvis_export_headers COMPONENT)
  set(SOURCE_DIR "${CMAKE_SOURCE_DIR}/src/${COMPONENT}/include")
    set(DEST_DIR   "${CMAKE_BINARY_DIR}/include/fccvis/")

    if(NOT EXISTS "${SOURCE_DIR}")
        message(WARNING
            "No include directory found for component '${COMPONENT}': "
            "${SOURCE_DIR}"
        )
        return()
    endif()

    file(GLOB_RECURSE HEADERS
        CONFIGURE_DEPENDS
        "${SOURCE_DIR}/*"
    )

    foreach(HEADER ${HEADERS})
        if(IS_DIRECTORY "${HEADER}")
            continue()
        endif()

        file(RELATIVE_PATH RELATIVE "${SOURCE_DIR}" "${HEADER}")

        set(DESTINATION
            "${DEST_DIR}/${RELATIVE}"
        )

        # Create the parent directory.
        get_filename_component(DEST_PARENT "${DESTINATION}" DIRECTORY)

        file(MAKE_DIRECTORY "${DEST_PARENT}")

        # Symlink instead of copying.
        file(CREATE_LINK
            "${HEADER}"
            "${DESTINATION}"
            SYMBOLIC
        )
    endforeach()
endfunction()
