# - Find hiredis
# This module defines:
#   HIREDIS_FOUND        - system has hiredis
#   HIREDIS_INCLUDE_DIRS - the hiredis include directory
#   HIREDIS_LIBRARIES    - the hiredis library to link

find_path(HIREDIS_INCLUDE_DIR
    NAMES hiredis/hiredis.h
    PATHS /usr/include /usr/local/include
)

find_library(HIREDIS_LIBRARY
    NAMES hiredis
    PATHS /usr/lib /usr/local/lib /usr/lib/aarch64-linux-gnu
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(hiredis
    DEFAULT_MSG
    HIREDIS_INCLUDE_DIR HIREDIS_LIBRARY
)

# message("HIREDIS_INCLUDE_DIR = ${HIREDIS_INCLUDE_DIR}")
# message("HIREDIS_LIBRARY = ${HIREDIS_LIBRARY}")

if(HIREDIS_FOUND)
    set(HIREDIS_INCLUDE_DIRS ${HIREDIS_INCLUDE_DIR})
    set(HIREDIS_LIBRARIES ${HIREDIS_LIBRARY})
endif()

mark_as_advanced(HIREDIS_INCLUDE_DIR HIREDIS_LIBRARY)
