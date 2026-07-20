#include <uuid_map/uuid_map.hpp>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

void write_from_another_translation_unit(std::uint64_t actor_hi, std::uint32_t actor_lo,
                                         std::uint32_t local_index, std::uint32_t value);

namespace {

namespace map = sovereignbase::uuid_map;

struct point {
  std::uint64_t actor_hi;
  std::uint32_t actor_lo;
  std::uint32_t local_index;
};

[[nodiscard]] point key(std::uint64_t actor_hi, std::uint32_t actor_lo,
                        std::uint32_t local_index) noexcept {
  return {actor_hi, actor_lo, local_index};
}

[[nodiscard]] std::uint32_t read(point value) noexcept {
  return map::read(value.actor_hi, value.actor_lo, value.local_index);
}

void write(point destination, std::uint32_t value) {
  map::write(destination.actor_hi, destination.actor_lo, destination.local_index, value);
}

void remove(point value) { map::remove(value.actor_hi, value.actor_lo, value.local_index); }

[[noreturn]] void fail(const char* expression, int line) {
  throw std::runtime_error(std::string("line ") + std::to_string(line) +
                           ": check failed: " + expression);
}

#define CHECK(expression)                                                                          \
  do {                                                                                             \
    if (!(expression))                                                                             \
      fail(#expression, __LINE__);                                                                 \
  } while (false)

void basic_operations() {
  const auto first = key(1, 2, 3);
  const auto second = key(1, 2, 4);
  write(first, 10);
  write(second, 20);
  CHECK(read(first) == 10);
  CHECK(read(second) == 20);
  write(first, 0);
  CHECK(read(first) == 0);
  remove(first);
  remove(first);
  CHECK(read(second) == 20);
  remove(second);
}

void shared_state() {
  const auto shared = key(7, 8, 9);
  write_from_another_translation_unit(shared.actor_hi, shared.actor_lo, shared.local_index, 42);
  CHECK(read(shared) == 42);
  remove(shared);
}

void actors_and_indices() {
  constexpr std::size_t actors = 256;
  constexpr std::size_t indices = 1024;

  for (std::size_t actor = 0; actor < actors; ++actor)
    for (std::size_t index = 0; index < indices; ++index)
      write(key(actor * UINT64_C(0x9e3779b97f4a7c15), std::uint32_t(actor), std::uint32_t(index)),
            std::uint32_t(actor ^ index));

  for (std::size_t actor = 0; actor < actors; ++actor)
    for (std::size_t index = 0; index < indices; ++index)
      CHECK(read(key(actor * UINT64_C(0x9e3779b97f4a7c15), std::uint32_t(actor),
                     std::uint32_t(index))) == std::uint32_t(actor ^ index));

  for (std::size_t actor = 0; actor < actors; ++actor)
    for (std::size_t index = indices; index-- > 0;)
      remove(key(actor * UINT64_C(0x9e3779b97f4a7c15), std::uint32_t(actor), std::uint32_t(index)));
}

void collision_chains() {
  constexpr std::size_t count = 96;
  for (std::size_t actor = 0; actor < count; ++actor)
    write(key(actor, std::uint32_t(actor), 0), std::uint32_t(actor + 1));

  for (std::size_t actor = 0; actor < 64; ++actor)
    remove(key(actor, std::uint32_t(actor), 0));
  for (std::size_t actor = 64; actor < count; ++actor)
    CHECK(read(key(actor, std::uint32_t(actor), 0)) == actor + 1);
  for (std::size_t actor = 64; actor < count; ++actor)
    remove(key(actor, std::uint32_t(actor), 0));
}

void value_storage_shrink() {
  constexpr std::size_t count = 4096;
  const auto actor = key(UINT64_C(0x123456789abcdef0), UINT32_C(0x12345678), 0);

  for (std::size_t index = 0; index < count; ++index)
    write(key(actor.actor_hi, actor.actor_lo, std::uint32_t(index)), std::uint32_t(index));

  std::size_t slot = map::detail::find(actor.actor_hi, actor.actor_lo);
  const std::size_t capacity_before = map::detail::metadata[slot].capacity;
  for (std::size_t index = count; index-- > 64;)
    remove(key(actor.actor_hi, actor.actor_lo, std::uint32_t(index)));

  slot = map::detail::find(actor.actor_hi, actor.actor_lo);
  CHECK(map::detail::metadata[slot].capacity < capacity_before);
  for (std::size_t index = 0; index < 64; ++index)
    CHECK(read(key(actor.actor_hi, actor.actor_lo, std::uint32_t(index))) == index);
  for (std::size_t index = 0; index < 64; ++index)
    remove(key(actor.actor_hi, actor.actor_lo, std::uint32_t(index)));
}

void stress() {
  constexpr std::size_t count = 512;
  std::array<bool, count> present{};
  std::array<std::uint32_t, count> values{};
  std::uint64_t random = UINT64_C(0x6a09e667f3bcc909);

  for (std::size_t operation = 0; operation < 100000; ++operation) {
    random ^= random << 13;
    random ^= random >> 7;
    random ^= random << 17;
    const std::size_t i = std::size_t(random % count);
    const auto point = key(i % 17, std::uint32_t(i % 17), std::uint32_t(i / 17));

    if ((random >> 16) % 3 == 0) {
      values[i] = std::uint32_t(random);
      write(point, values[i]);
      present[i] = true;
    } else if ((random >> 16) % 3 == 1) {
      remove(point);
      present[i] = false;
    } else if (present[i]) {
      CHECK(read(point) == values[i]);
    }
  }

  for (std::size_t i = 0; i < count; ++i)
    if (present[i])
      remove(key(i % 17, std::uint32_t(i % 17), std::uint32_t(i / 17)));
}

} // namespace

int main() {
  try {
    basic_operations();
    shared_state();
    actors_and_indices();
    collision_chains();
    value_storage_shrink();
    stress();
    CHECK(map::detail::actors == nullptr);
    CHECK(map::detail::metadata == nullptr);
    CHECK(map::detail::actor_count == 0);
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
