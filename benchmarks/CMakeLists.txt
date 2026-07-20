include(FetchContent)

# Keep dependency test suites out of the uuid_map benchmark build.
set(BUILD_TESTING OFF)

find_package(unordered_dense 4.8.1 EXACT CONFIG QUIET)
if(NOT TARGET unordered_dense::unordered_dense)
  set(ANKERL_ENABLE_EXAMPLE OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    unordered_dense
    GIT_REPOSITORY https://github.com/martinus/unordered_dense.git
    GIT_TAG 3234af2c03549bc85656bfd3a86993bf1cd8aef1
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(unordered_dense)
endif()

find_package(boost_unordered 1.91.0 EXACT CONFIG QUIET)
if(NOT TARGET Boost::unordered)
  set(BOOST_SUPERPROJECT_VERSION 1.91.0)

  function(uuid_map_fetch_boost module hash)
    FetchContent_Declare(
      boost_${module}
      URL https://github.com/boostorg/${module}/archive/refs/tags/boost-1.91.0.tar.gz
      URL_HASH SHA256=${hash}
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(boost_${module})
  endfunction()

  uuid_map_fetch_boost(assert 9145fba14048a46c0f65e5b28e68176d568148957ebf9bdf9e82bcc5d5a703a9)
  uuid_map_fetch_boost(config e69618fa862927db69b4dd8e6b070c647a799b892fd0ee141d28db0dab025531)
  uuid_map_fetch_boost(mp11 6ab871e0ef397a2e7b0602f13a5f473a7381043c91b57fe907ca4925a6a1ee58)
  uuid_map_fetch_boost(describe 755e216f0f36379dc87e32ff5ab16d87fa9df4276562f241669924298899fc2b)
  uuid_map_fetch_boost(container_hash 5ec8bf37a75bef0bbac5f9e6e5f95d22a8e8a0034ce225d40ba701a483b34d27)
  uuid_map_fetch_boost(throw_exception bba826d1380ccedbcf0468ae4b74012ac14c3830be30d9174fbaf8583b56ed67)
  uuid_map_fetch_boost(core fbc69a21a0b3c839a2657ed803f1e7c1e6426d3d6e7c8bddb6b7a498382b5cd1)
  uuid_map_fetch_boost(predef 8b2444f89b34a39745177f3eb6d3753bd4647dfffeb7aecfa32ee9cc1958bb33)
  uuid_map_fetch_boost(unordered d35da719683cddd2aa47d953ebbc54798b3e21d0475b5f10be22562bf1117392)
endif()

add_executable(uuid_map_benchmark uuid_map_benchmark.cpp)
target_compile_features(uuid_map_benchmark PRIVATE cxx_std_23)
target_link_libraries(
  uuid_map_benchmark
  PRIVATE
    sovereignbase::uuid_map
    unordered_dense::unordered_dense
    Boost::unordered
)

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
