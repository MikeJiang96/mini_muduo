file(GLOB_RECURSE ALL_SRC_FILES CONFIGURE_DEPENDS
     ${CMAKE_SOURCE_DIR}/include/*.h
     ${CMAKE_SOURCE_DIR}/src/*.h
     ${CMAKE_SOURCE_DIR}/src/*.cpp
     ${CMAKE_SOURCE_DIR}/test/*.h
     ${CMAKE_SOURCE_DIR}/test/*.cpp
     ${CMAKE_SOURCE_DIR}/examples/*.h
     ${CMAKE_SOURCE_DIR}/examples/*.cpp)

add_custom_target(format "clang-format" -i ${ALL_SRC_FILES} COMMENT "Formatting source code...")
