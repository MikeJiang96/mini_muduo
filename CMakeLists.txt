cmake_minimum_required(VERSION 3.15)
project(mini_muduo VERSION 0.1.0 LANGUAGES C CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Werror)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -fno-omit-frame-pointer")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS "include/*.h" "src/*.h" "src/*.cpp")

add_library(mini_muduo SHARED ${SRC_FILES})

target_compile_options(mini_muduo
    PRIVATE $<$<COMPILE_LANGUAGE:CXX>: -Wconversion -Wshadow>
)

target_include_directories(mini_muduo PUBLIC include)

find_package(Threads REQUIRED)
target_link_libraries(mini_muduo PUBLIC Threads::Threads)

find_package(spdlog REQUIRED)
target_link_libraries(mini_muduo PUBLIC spdlog::spdlog)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_subdirectory(examples)
add_subdirectory(test)

include(cmake/scanners.cmake)
