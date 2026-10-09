set(LOCAL_ENABLED_BACKENDS "")

if(BUILD_BACKEND_FV)
    add_subdirectory(fv)
    list(APPEND LOCAL_ENABLED_BACKENDS fccvis_backend_fv)
endif()

# Propagate the collected targets list up to backend/CMakeLists.txt
set(ENABLED_BACKEND_TARGETS ${LOCAL_ENABLED_BACKENDS} PARENT_SCOPE)
