add_executable(uuid_map_benchmark uuid_map_benchmark.cpp)
target_link_libraries(uuid_map_benchmark PRIVATE sovereignbase::uuid_map)

if(MSVC)
  target_compile_options(uuid_map_benchmark PRIVATE /W4 /permissive-)
else()
  target_compile_options(
    uuid_map_benchmark
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      $<$<CONFIG:Release>:-march=native>
  )
endif()
