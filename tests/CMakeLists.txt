add_executable(uuid_map_tests uuid_map_test.cpp shared_state_writer.cpp)
target_link_libraries(uuid_map_tests PRIVATE sovereignbase::uuid_map)

if(MSVC)
  target_compile_options(uuid_map_tests PRIVATE /W4 /permissive-)
else()
  target_compile_options(uuid_map_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME uuid_map.tests COMMAND uuid_map_tests)
