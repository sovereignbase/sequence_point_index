#include <uuid_map/uuid_map.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

namespace {

namespace map = sovereignbase::uuid_map;
using clock_type = std::chrono::steady_clock;

constexpr std::array<std::size_t, 15> entry_counts{
    1,      100,     250,     500,     1'000,     5'000,     10'000,    25'000,
    50'000, 100'000, 250'000, 500'000, 1'000'000, 5'000'000, 10'000'000};
constexpr std::array<std::size_t, entry_counts.size()> actor_counts{
    1, 79, 116, 155, 786, 595, 394, 325, 290, 195, 88, 218, 869, 542, 464};
constexpr std::size_t sample_count = 5;
constexpr std::size_t target_operations = 10'000'000;

volatile std::uint64_t benchmark_sink = 0;

struct actor_id {
  std::uint64_t high;
  std::uint32_t low;
};

struct sequence_point {
  std::uint64_t actor_hi;
  std::uint32_t actor_lo;
  std::uint32_t local_index;
};

struct resolved_point {
  std::uint32_t* values;
  std::uint32_t index;
};

struct measurement {
  double read;
  double resolved_read;
  double write;
  double remove;
};

[[nodiscard]] std::uint64_t splitmix64(std::uint64_t& state) noexcept {
  std::uint64_t value = (state += UINT64_C(0x9e3779b97f4a7c15));
  value = (value ^ (value >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
  value = (value ^ (value >> 27)) * UINT64_C(0x94d049bb133111eb);
  return value ^ (value >> 31);
}

[[nodiscard]] sequence_point make_key(actor_id actor, std::uint32_t index) noexcept {
  return {actor.high, actor.low, index};
}

[[nodiscard]] std::vector<sequence_point> make_keys(std::size_t count, std::size_t actor_count) {
  std::uint64_t random = UINT64_C(0x243f6a8885a308d3);
  std::vector<actor_id> actors(actor_count);
  std::vector<std::uint32_t> next_index(actor_count);
  std::vector<sequence_point> keys;
  keys.reserve(count);

  for (actor_id& actor : actors) {
    actor.high = splitmix64(random);
    actor.low = std::uint32_t(splitmix64(random));
  }
  for (std::size_t i = 0; i < count; ++i) {
    const std::size_t actor = splitmix64(random) % actor_count;
    keys.push_back(make_key(actors[actor], next_index[actor]++));
  }
  return keys;
}

void populate(const std::vector<sequence_point>& keys) {
  for (std::size_t i = 0; i < keys.size(); ++i)
    map::write(keys[i].actor_hi, keys[i].actor_lo, keys[i].local_index, std::uint32_t(i));
}

[[nodiscard]] std::uint32_t* resolve(sequence_point key) noexcept {
  std::uint32_t slot = map::detail::hash(key.actor_hi, key.actor_lo) & map::detail::actor_mask;
  while (map::detail::actors[slot].key_hi != key.actor_hi ||
         map::detail::actors[slot].key_lo != key.actor_lo)
    slot = (slot + 1) & map::detail::actor_mask;
  return map::detail::actors[slot].values;
}

template <typename Function>
[[nodiscard]] double median_ns(std::size_t entries, Function function) {
  const std::size_t repeats = std::max<std::size_t>(1, target_operations / entries);
  const std::size_t operations = entries * repeats;
  std::array<double, sample_count> samples{};

  benchmark_sink ^= function(1);
  for (double& sample : samples) {
    const auto start = clock_type::now();
    benchmark_sink ^= function(repeats);
    const auto elapsed = std::chrono::duration<double, std::nano>(clock_type::now() - start);
    sample = elapsed.count() / double(operations);
  }
  std::sort(samples.begin(), samples.end());
  return samples[sample_count / 2];
}

[[nodiscard]] double measure_remove(const std::vector<sequence_point>& keys) {
  std::array<double, sample_count> samples{};
  for (double& sample : samples) {
    const auto start = clock_type::now();
    for (const sequence_point key : keys)
      map::remove(key.actor_hi, key.actor_lo, key.local_index);
    const auto elapsed = std::chrono::duration<double, std::nano>(clock_type::now() - start);
    sample = elapsed.count() / double(keys.size());
    populate(keys);
  }
  std::sort(samples.begin(), samples.end());
  for (const sequence_point key : keys)
    map::remove(key.actor_hi, key.actor_lo, key.local_index);
  return samples[sample_count / 2];
}

[[nodiscard]] measurement measure(const std::vector<sequence_point>& keys) {
  populate(keys);

  std::vector<resolved_point> resolved;
  resolved.reserve(keys.size());
  for (const sequence_point key : keys)
    resolved.push_back({resolve(key), key.local_index});

  const double read = median_ns(keys.size(), [&](std::size_t repeats) {
    std::uint64_t sum = 0;
    for (std::size_t repeat = 0; repeat < repeats; ++repeat)
      for (const sequence_point key : keys)
        sum += map::read(key.actor_hi, key.actor_lo, key.local_index);
    return sum;
  });

  const double resolved_read = median_ns(resolved.size(), [&](std::size_t repeats) {
    std::uint64_t sum = 0;
    for (std::size_t repeat = 0; repeat < repeats; ++repeat)
      for (const resolved_point point : resolved)
        sum += point.values[point.index];
    return sum;
  });

  std::uint32_t generation = 0;
  const double write = median_ns(keys.size(), [&](std::size_t repeats) {
    for (std::size_t repeat = 0; repeat < repeats; ++repeat) {
      ++generation;
      for (const sequence_point key : keys)
        map::write(key.actor_hi, key.actor_lo, key.local_index, generation);
    }
    const sequence_point key = keys.back();
    return std::uint64_t(map::read(key.actor_hi, key.actor_lo, key.local_index));
  });

  return {read, resolved_read, write, measure_remove(keys)};
}

} // namespace

int main() {
  std::array<measurement, entry_counts.size()> results{};

  for (std::size_t row = 0; row < entry_counts.size(); ++row) {
    const auto keys = make_keys(entry_counts[row], actor_counts[row]);
    results[row] = measure(keys);
  }

  std::cout << "\nsequence-point operations (ns/op, median of " << sample_count
            << ", actor-interleaved sequence order)\n"
            << "| IDs | actors | read | resolved pointer read | write existing | remove |\n"
            << "|---:|---:|---:|---:|---:|---:|\n";

  double read_average = 0;
  double resolved_average = 0;
  for (std::size_t row = 0; row < results.size(); ++row) {
    const measurement result = results[row];
    read_average += result.read;
    resolved_average += result.resolved_read;
    std::cout << "| " << entry_counts[row] << " | " << actor_counts[row] << std::fixed
              << std::setprecision(2) << " | " << result.read << " | " << result.resolved_read
              << " | " << result.write << " | " << result.remove << " |\n";
  }

  read_average /= double(results.size());
  resolved_average /= double(results.size());
  std::cout << "\nread arithmetic mean: " << read_average << " ns/op\n"
            << "resolved arithmetic mean: " << resolved_average << " ns/op\n"
            << "10M read: " << results.back().read << " ns/op\n"
            << "10M write: " << results.back().write << " ns/op\n"
            << "10M remove: " << results.back().remove << " ns/op\n";
  return 0;
}
