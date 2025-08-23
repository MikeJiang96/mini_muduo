add_library(mini_muduo_hiredis_demo SHARED
    hiredis_wrapper.h hiredis_wrapper.cpp
    redis_cache_client.h redis_cache_client.cpp)

target_include_directories(mini_muduo_hiredis_demo PUBLIC ${HIREDIS_INCLUDE_DIRS})
target_link_libraries(mini_muduo_hiredis_demo PUBLIC ${HIREDIS_LIBRARY})
target_link_libraries(mini_muduo_hiredis_demo PUBLIC mini_muduo)

add_executable(hiredis_demo_test main.cpp)
target_link_libraries(hiredis_demo_test PRIVATE mini_muduo_hiredis_demo)
