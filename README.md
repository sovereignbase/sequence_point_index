# sequence_point_index

A header-only, zero-dependency C++ registry for 128-bit sequence-point addresses:

```text
[ actor word 0 ][ actor word 1 ][ actor word 2 ][ local index ]
       32-bit          32-bit          32-bit          32-bit
```

> Why hash ten million addresses when only their actors need lookup?

```cpp
#include <uuid_map/uuid_map.hpp>

namespace uuid_map = sovereignbase::uuid_map;

uuid_map::write(actor_0, actor_1, actor_2, local_index, 42);
const std::uint32_t value = uuid_map::read(actor_0, actor_1, actor_2, local_index);
uuid_map::remove(actor_0, actor_1, actor_2, local_index);
```

There is no map object, key wrapper, template, handle, optional, or status object. C++17 inline
state provides one registry shared by all translation units in the linked program image.

## API

```cpp
std::uint32_t read(std::uint32_t actor_0,
                   std::uint32_t actor_1,
                   std::uint32_t actor_2,
                   std::uint32_t local_index) noexcept;

void write(std::uint32_t actor_0,
           std::uint32_t actor_1,
           std::uint32_t actor_2,
           std::uint32_t local_index,
           std::uint32_t value);

void remove(std::uint32_t actor_0,
            std::uint32_t actor_1,
            std::uint32_t actor_2,
            std::uint32_t local_index);
```

`write` inserts or replaces a value. Every `std::uint32_t` value, including zero, is valid.
`remove` is a no-op for a missing address. `read` requires that the address has been written and
not removed; violating that precondition is undefined behavior so the hot path needs no branch or
sentinel.

The registry is intentionally unsynchronized. A runtime that accesses it from multiple threads
must provide external synchronization around reads and mutations.

## Layout

Only the 96-bit actor is hashed. Its three 32-bit words are XOR-folded, masked into a power-of-two
table, and collision slots are probed linearly. A 24-byte actor slot contains the full actor ID and
the actor's value pointer. A successful read ends with exactly:

```cpp
return actor.values[local_index];
```

The actor table grows at 50% occupancy, shrinks at 12.5%, and disappears when empty. Each actor's
contiguous value storage grows geometrically. Removing the highest live indices shrinks that
storage automatically; removing the actor's final value releases it.

A cold one-bit-per-value presence bitmap is touched only by `write` and `remove`. It allows zero to
remain a valid value and never adds work to `read`.

This direct layout is intended for dense or reasonably bounded actor-local indices. A single very
large sparse index necessarily requests a correspondingly large contiguous allocation; adding
paging would add another dependent memory lookup to every read.

## CMake

```cmake
include(FetchContent)

FetchContent_Declare(
  uuid_map
  GIT_REPOSITORY https://github.com/sovereignbase/uuid_map.git
  GIT_TAG main
)
FetchContent_MakeAvailable(uuid_map)

target_link_libraries(your_target PRIVATE sovereignbase::uuid_map)
```

The same target works with `add_subdirectory` and an installed
`find_package(uuid_map CONFIG REQUIRED)` package. The only requirement is C++17; there are no
third-party or generated dependencies.

Because the state uses header-only inline variables, it is shared across translation units in one
executable or shared-library image. Separate DLL/DSO images require one exported compiled state if
they must share a process-wide registry.

## Development

```sh
cmake --preset release -DUUID_MAP_BUILD_BENCHMARKS=ON
cmake --build --preset release
ctest --preset release
./build/release/benchmarks/uuid_map_benchmark
```

The benchmark reports nanoseconds per operation from 1,000 through 10 million IDs and compares
`uuid_map` with ankerl, Boost, `std::unordered_map`, and C++23 `std::flat_map`. Its workload,
dependency isolation, and reproducibility notes are documented in
[benchmarks/README.md](benchmarks/README.md).

Licensed under the Apache License 2.0.
