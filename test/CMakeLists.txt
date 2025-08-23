add_executable(event_loop_unittest event_loop_unittest.cpp)
target_link_libraries(event_loop_unittest mini_muduo)
add_test(NAME event_loop_unittest COMMAND event_loop_unittest)

add_executable(timer_queue_unittest timer_queue_unittest.cpp)
target_link_libraries(timer_queue_unittest mini_muduo)
add_test(NAME timer_queue_unittest COMMAND timer_queue_unittest)

find_package(Boost QUIET COMPONENTS unit_test_framework)
if(Boost_UNIT_TEST_FRAMEWORK_FOUND)
    add_executable(buffer_unittest buffer_unittest.cpp)
    target_link_libraries(buffer_unittest mini_muduo Boost::unit_test_framework)
    add_test(NAME buffer_unittest COMMAND buffer_unittest)
endif()
