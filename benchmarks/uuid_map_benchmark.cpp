#include <uuid_map/uuid_map.hpp>

#include <ankerl/unordered_dense.h>
#include <boost/unordered/unordered_flat_map.hpp>

#if __has_include(<flat_map>)
#include <flat_map>
#if defined(__cpp_lib_flat_map) && __cpp_lib_flat_map >= 202207L
#define UUID_MAP_HAS_STD_FLAT_MAP 1
#endif
#endif

#ifndef UUID_MAP_HAS_STD_FLAT_MAP
#define UUID_MAP_HAS_STD_FLAT_MAP 0
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

namespace uuid_map = sovereignbase::uuid_map;
using clock_type = std::chrono::steady_clock;

constexpr std::array<std::size_t, 9> entry_counts{1'000,   10'000,    50'000,    100'000,   250'000,
                                                  500'000, 1'000'000, 5'000'000, 10'000'000};
constexpr std::array<std::size_t, entry_counts.size()> actor_counts{786, 394, 290, 195, 88,
                                                                    218, 869, 542, 464};
constexpr std::size_t sample_count = 3;
constexpr std::size_t target_operations = 3'000'000;
constexpr std::size_t quadratic_mutation_limit = 10'000;
constexpr std::size_t implementation_count = 5;

constexpr std::array<std::string_view, implementation_count> implementation_names{
    "uuid_map", "ankerl::unordered_dense", "boost::unordered_flat_map", "std::unordered_map",
    "std::flat_map"};

volatile std::uint64_t benchmark_sink = 0;

struct actor_id {
  std::uint32_t word_0;
  std::uint32_t word_1;
  std::uint32_t word_2;
};

struct sequence_point {
  std::uint32_t actor_0;
  std::uint32_t actor_1;
  std::uint32_t actor_2;
  std::uint32_t local_index;

  friend bool operator==(const sequence_point&, const sequence_point&) = default;
};

static_assert(sizeof(sequence_point) == 16);
static_assert(alignof(sequence_point) == alignof(std::uint32_t));

struct sequence_less {
  [[nodiscard]] bool operator()(const sequence_point& left,
                                const sequence_point& right) const noexcept {
    if (left.actor_0 != right.actor_0)
      return left.actor_0 < right.actor_0;
    if (left.actor_1 != right.actor_1)
      return left.actor_1 < right.actor_1;
    if (left.actor_2 != right.actor_2)
      return left.actor_2 < right.actor_2;
    return left.local_index < right.local_index;
  }
};

struct sequence_hash {
  using is_avalanching = void;

  [[nodiscard]] std::size_t operator()(const sequence_point& key) const noexcept {
    std::uint64_t value = (std::uint64_t{key.actor_0} << 32) ^ key.actor_1 ^
                          (std::uint64_t{key.actor_2} << 32) ^ key.local_index;
    value = (value ^ (value >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    value = (value ^ (value >> 27)) * UINT64_C(0x94d049bb133111eb);
    return std::size_t(value ^ (value >> 31));
  }
};

[[nodiscard]] std::uint32_t initial_value(const sequence_point& key) noexcept {
  return key.actor_0 ^ key.actor_1 ^ key.actor_2 ^ key.local_index;
}

[[nodiscard]] std::uint64_t splitmix64(std::uint64_t& state) noexcept {
  std::uint64_t value = (state += UINT64_C(0x9e3779b97f4a7c15));
  value = (value ^ (value >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
  value = (value ^ (value >> 27)) * UINT64_C(0x94d049bb133111eb);
  return value ^ (value >> 31);
}

[[nodiscard]] std::vector<sequence_point> make_keys(std::size_t count, std::size_t actor_count) {
  if (actor_count > count)
    throw std::runtime_error("actor count exceeds ID count");

  std::uint64_t random = UINT64_C(0x243f6a8885a308d3);
  std::vector<actor_id> actors(actor_count);
  std::vector<std::size_t> actor_order;
  std::vector<std::uint32_t> next_index(actor_count);
  std::vector<sequence_point> keys;
  actor_order.reserve(count);
  keys.reserve(count);

  for (actor_id& actor : actors) {
    const std::uint64_t first_words = splitmix64(random);
    actor.word_0 = std::uint32_t(first_words >> 32);
    actor.word_1 = std::uint32_t(first_words);
    actor.word_2 = std::uint32_t(splitmix64(random));
  }
  for (std::size_t actor = 0; actor < actor_count; ++actor)
    actor_order.push_back(actor);
  while (actor_order.size() < count)
    actor_order.push_back(splitmix64(random) % actor_count);
  for (std::size_t i = actor_order.size(); i > 1; --i)
    std::swap(actor_order[i - 1], actor_order[splitmix64(random) % i]);

  for (const std::size_t actor : actor_order)
    keys.push_back(
        {actors[actor].word_0, actors[actor].word_1, actors[actor].word_2, next_index[actor]++});
  return keys;
}

class uuid_store {
public:
  static constexpr bool quadratic_mutation = false;

  void prepare(const std::vector<sequence_point>& keys) {
    for (const sequence_point& key : keys)
      insert(key, initial_value(key));
  }

  void insert(const sequence_point& key, std::uint32_t value) {
    uuid_map::write(key.actor_0, key.actor_1, key.actor_2, key.local_index, value);
  }

  [[nodiscard]] std::uint32_t read(const sequence_point& key) const noexcept {
    return uuid_map::read(key.actor_0, key.actor_1, key.actor_2, key.local_index);
  }

  void update(const sequence_point& key, std::uint32_t value) { insert(key, value); }

  void erase(const sequence_point& key) {
    uuid_map::remove(key.actor_0, key.actor_1, key.actor_2, key.local_index);
  }

  void clear(const std::vector<sequence_point>& keys) {
    for (const sequence_point& key : keys)
      erase(key);
  }

  [[nodiscard]] bool empty() const noexcept { return uuid_map::detail::actor_count == 0; }
};

template <typename Map> class hash_store {
public:
  static constexpr bool quadratic_mutation = false;

  void prepare(const std::vector<sequence_point>& keys) {
    values_.clear();
    values_.reserve(keys.size());
    for (const sequence_point& key : keys)
      insert(key, initial_value(key));
  }

  void insert(const sequence_point& key, std::uint32_t value) { values_.emplace(key, value); }

  [[nodiscard]] std::uint32_t read(const sequence_point& key) const noexcept {
    return values_.find(key)->second;
  }

  void update(const sequence_point& key, std::uint32_t value) {
    values_.insert_or_assign(key, value);
  }

  void erase(const sequence_point& key) { values_.erase(key); }

  void clear(const std::vector<sequence_point>&) { values_.clear(); }

  [[nodiscard]] bool empty() const noexcept { return values_.empty(); }

private:
  Map values_;
};

using dense_store =
    hash_store<ankerl::unordered_dense::map<sequence_point, std::uint32_t, sequence_hash>>;
using boost_store =
    hash_store<boost::unordered_flat_map<sequence_point, std::uint32_t, sequence_hash>>;
using standard_store = hash_store<std::unordered_map<sequence_point, std::uint32_t, sequence_hash>>;

#if UUID_MAP_HAS_STD_FLAT_MAP
class flat_store {
public:
  static constexpr bool quadratic_mutation = true;

  void prepare(const std::vector<sequence_point>& keys) {
    std::vector<sequence_point> sorted_keys = keys;
    std::sort(sorted_keys.begin(), sorted_keys.end(), sequence_less{});

    std::vector<std::uint32_t> values;
    values.reserve(sorted_keys.size());
    for (const sequence_point& key : sorted_keys)
      values.push_back(initial_value(key));

    values_ =
        map_type(std::sorted_unique, std::move(sorted_keys), std::move(values), sequence_less{});
  }

  void insert(const sequence_point& key, std::uint32_t value) { values_.emplace(key, value); }

  [[nodiscard]] std::uint32_t read(const sequence_point& key) const noexcept {
    return values_.find(key)->second;
  }

  void update(const sequence_point& key, std::uint32_t value) {
    values_.insert_or_assign(key, value);
  }

  void erase(const sequence_point& key) { values_.erase(key); }

  void clear(const std::vector<sequence_point>&) { values_.clear(); }

  [[nodiscard]] bool empty() const noexcept { return values_.empty(); }

private:
  using map_type = std::flat_map<sequence_point, std::uint32_t, sequence_less>;
  map_type values_;
};
#endif

struct measurement {
  double read = std::numeric_limits<double>::quiet_NaN();
  double update = std::numeric_limits<double>::quiet_NaN();
  double insert = std::numeric_limits<double>::quiet_NaN();
  double remove = std::numeric_limits<double>::quiet_NaN();
};

template <typename Function>
[[nodiscard]] double repeated_median_ns(std::size_t entries, Function function) {
  const std::size_t repeats = std::max<std::size_t>(1, target_operations / entries);
  const std::size_t operations = entries * repeats;
  std::array<double, sample_count> samples{};

  benchmark_sink = benchmark_sink ^ function(1);
  for (double& sample : samples) {
    const auto start = clock_type::now();
    benchmark_sink = benchmark_sink ^ function(repeats);
    const auto elapsed = std::chrono::duration<double, std::nano>(clock_type::now() - start);
    sample = elapsed.count() / double(operations);
  }
  std::sort(samples.begin(), samples.end());
  return samples[sample_count / 2];
}

template <typename Store>
[[nodiscard]] double measure_read(Store& store, const std::vector<sequence_point>& keys) {
  std::uint64_t expected = 0;
  std::uint64_t actual = 0;
  for (const sequence_point& key : keys) {
    expected += initial_value(key);
    actual += store.read(key);
  }
  if (actual != expected)
    throw std::runtime_error("read validation failed");

  return repeated_median_ns(keys.size(), [&](std::size_t repeats) {
    std::uint64_t sum = 0;
    for (std::size_t repeat = 0; repeat < repeats; ++repeat)
      for (const sequence_point& key : keys)
        sum += store.read(key);
    return sum;
  });
}

template <typename Store>
[[nodiscard]] double measure_update(Store& store, const std::vector<sequence_point>& keys) {
  std::uint32_t generation = 0;
  const double result = repeated_median_ns(keys.size(), [&](std::size_t repeats) {
    for (std::size_t repeat = 0; repeat < repeats; ++repeat) {
      ++generation;
      for (const sequence_point& key : keys)
        store.update(key, generation);
      std::atomic_signal_fence(std::memory_order_seq_cst);
    }
    return std::uint64_t(generation);
  });
  for (const sequence_point& key : keys)
    if (store.read(key) != generation)
      throw std::runtime_error("update validation failed");
  return result;
}

template <typename Store>
[[nodiscard]] double measure_remove(Store& store, const std::vector<sequence_point>& keys) {
  std::array<double, sample_count> samples{};
  for (std::size_t sample = 0; sample < sample_count; ++sample) {
    if (sample != 0)
      store.prepare(keys);
    const auto start = clock_type::now();
    for (const sequence_point& key : keys)
      store.erase(key);
    const auto elapsed = std::chrono::duration<double, std::nano>(clock_type::now() - start);
    samples[sample] = elapsed.count() / double(keys.size());
    if (!store.empty())
      throw std::runtime_error("remove validation failed");
  }
  std::sort(samples.begin(), samples.end());
  return samples[sample_count / 2];
}

template <typename Store>
[[nodiscard]] double measure_insert(const std::vector<sequence_point>& keys) {
  std::array<double, sample_count> samples{};
  for (double& sample : samples) {
    Store store;
    const auto start = clock_type::now();
    for (const sequence_point& key : keys)
      store.insert(key, initial_value(key));
    const auto elapsed = std::chrono::duration<double, std::nano>(clock_type::now() - start);
    sample = elapsed.count() / double(keys.size());

    if (store.empty() || store.read(keys.back()) != initial_value(keys.back()))
      throw std::runtime_error("insert validation failed");
    store.clear(keys);
    if (!store.empty())
      throw std::runtime_error("insert cleanup failed");
  }
  std::sort(samples.begin(), samples.end());
  return samples[sample_count / 2];
}

template <typename Store>
[[nodiscard]] measurement measure(const std::vector<sequence_point>& keys) {
  measurement result;
  const bool measure_quadratic =
      !Store::quadratic_mutation || keys.size() <= quadratic_mutation_limit;

  {
    Store store;
    store.prepare(keys);
    result.read = measure_read(store, keys);
    result.update = measure_update(store, keys);
    if (measure_quadratic)
      result.remove = measure_remove(store, keys);
    else
      store.clear(keys);
  }

  if (measure_quadratic)
    result.insert = measure_insert<Store>(keys);
  return result;
}

using result_rows = std::array<std::array<measurement, implementation_count>, entry_counts.size()>;

void print_value(double value) {
  if (std::isfinite(value))
    std::cout << std::fixed << std::setprecision(2) << value;
  else
    std::cout << "n/a";
}

void print_table(std::string_view title, const result_rows& results, double measurement::* member) {
  std::cout << "\n" << title << " (ns/op)\n| IDs | actors";
  for (const std::string_view name : implementation_names)
    std::cout << " | " << name;
  std::cout << " |\n|---:|---:";
  for (std::size_t implementation = 0; implementation < implementation_count; ++implementation)
    std::cout << "|---:";
  std::cout << "|\n";

  std::array<double, implementation_count> sums{};
  std::array<std::size_t, implementation_count> counts{};
  for (std::size_t row = 0; row < entry_counts.size(); ++row) {
    std::cout << "| " << entry_counts[row] << " | " << actor_counts[row];
    for (std::size_t implementation = 0; implementation < implementation_count; ++implementation) {
      const double value = results[row][implementation].*member;
      std::cout << " | ";
      print_value(value);
      if (std::isfinite(value)) {
        sums[implementation] += value;
        ++counts[implementation];
      }
    }
    std::cout << " |\n";
  }

  std::cout << "| arithmetic mean | -";
  for (std::size_t implementation = 0; implementation < implementation_count; ++implementation) {
    std::cout << " | ";
    if (counts[implementation] == entry_counts.size())
      print_value(sums[implementation] / double(counts[implementation]));
    else
      std::cout << "n/a";
  }
  std::cout << " |\n";
}

} // namespace

int main() {
  try {
    result_rows results{};

    std::cout << "same 4 x uint32_t keys + uint32_t values\n"
              << "median of " << sample_count
              << ", actor-interleaved sequence order, lower is better\n";

    for (std::size_t row = 0; row < entry_counts.size(); ++row) {
      const auto keys = make_keys(entry_counts[row], actor_counts[row]);
      std::cerr << entry_counts[row] << " IDs:";

      for (std::size_t offset = 0; offset < implementation_count; ++offset) {
        const std::size_t implementation = (row + offset) % implementation_count;
        std::cerr << ' ' << implementation_names[implementation] << std::flush;
        switch (implementation) {
        case 0:
          results[row][implementation] = measure<uuid_store>(keys);
          break;
        case 1:
          results[row][implementation] = measure<dense_store>(keys);
          break;
        case 2:
          results[row][implementation] = measure<boost_store>(keys);
          break;
        case 3:
          results[row][implementation] = measure<standard_store>(keys);
          break;
        case 4:
#if UUID_MAP_HAS_STD_FLAT_MAP
          results[row][implementation] = measure<flat_store>(keys);
#endif
          break;
        default:
          break;
        }
      }
      std::cerr << '\n';
    }

    print_table("successful read", results, &measurement::read);
    print_table("existing write/update", results, &measurement::update);
    print_table("empty-to-N autogrow insert", results, &measurement::insert);
    print_table("successful remove", results, &measurement::remove);

#if UUID_MAP_HAS_STD_FLAT_MAP
    std::cout << "\nstd::flat_map insert/remove above " << quadratic_mutation_limit
              << " IDs: n/a because a full random-order sweep is O(N^2).\n";
#else
    std::cout << "\nstd::flat_map: n/a because this standard library has no C++23 <flat_map>.\n";
#endif
    std::cout << "Read/update setup uses reserve for generic hash maps and bulk-sorted setup for "
                 "std::flat_map.\n"
              << "Autogrow insert starts empty and unreserved for every implementation.\n"
              << "uuid_map remove includes its automatic value and actor storage shrinking.\n";
  } catch (const std::exception& error) {
    std::cerr << "benchmark failed: " << error.what() << '\n';
    return 1;
  }
  return 0;
}
