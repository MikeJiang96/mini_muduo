include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(cli_client)
add_subdirectory(echo)

find_package(hiredis QUIET)
if(HIREDIS_FOUND)
    add_subdirectory(hiredis_demo)
endif()
