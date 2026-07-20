# Benchmarks

The comparison benchmark runs the same four-word sequence-point keys and `std::uint32_t` values
through five implementations. Every key is exactly four `std::uint32_t` words: three actor words
and one actor-local index.

- `sovereignbase::uuid_map`
- `ankerl::unordered_dense::map` 4.8.1
- `boost::unordered_flat_map` 1.91.0
- `std::unordered_map`
- C++23 `std::flat_map`

Only the benchmark target fetches ankerl and the required Boost.Unordered header modules. The
installed `sovereignbase::uuid_map` library remains C++17 and dependency-free. Every fetched
revision or archive is pinned, and every Boost archive has a SHA-256 check.

## Fairness

Every implementation receives the same keys, values, actor counts, and actor-interleaved query
order. Every declared actor occurs at least once. The generic hash maps use the same full-128-bit
avalanche hasher. `uuid_map` intentionally uses its specialized actor-only lookup followed by
direct `values[local_index]`; measuring the benefit of that known key structure is the purpose of
the comparison.

Four operations are reported separately:

- successful read
- existing-key write/upsert
- empty-to-N insert with no reserve, including automatic growth
- successful remove of every key

Generic hash maps use `reserve(N)` only while preparing the steady-state read and update tests.
The insert test starts with an empty, unreserved container for everyone. `std::flat_map` is built
from bulk-sorted vectors for read/update; its random-order insert/remove sweeps are measured only
through 10,000 IDs because a full sweep is O(N^2).

Each number is the median of three samples. Read and update process at least three million
operations per sample, or every key once when the data set is larger. Insert and remove process
the entire data set once per sample. Implementation order rotates between size rows to reduce
systematic cold-start and thermal bias. Results are validated during the run; update sweeps have a
compiler barrier between generations and every final value is checked outside the timed region.

## Reference result

Intel Core i5-10210U, Windows x64, GCC 16.1.0, Release with
`-O3 -DNDEBUG -march=native`. Every value below is nanoseconds per operation. Each mean is the
unweighted arithmetic mean of the nine size rows.

| Implementation | Read mean | 10M read | Update mean | 10M update | Insert mean | 10M insert | Remove mean | 10M remove |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| uuid_map | **11.49** | **11.60** | **18.35** | **20.43** | **56.89** | **26.04** | **48.19** | **18.48** |
| ankerl::unordered_dense | 52.94 | 61.89 | 78.95 | 149.41 | 170.42 | 207.18 | 92.35 | 137.74 |
| boost::unordered_flat_map | 81.79 | 105.48 | 112.35 | 201.63 | 120.98 | 184.47 | 119.16 | 207.06 |
| std::unordered_map | 171.89 | 314.10 | 212.29 | 378.71 | 680.25 | 920.24 | 339.36 | 528.13 |
| std::flat_map | 369.80 | 544.75 | 336.21 | 586.89 | n/a | n/a | n/a | n/a |

At 10 million IDs, `uuid_map` read was 5.3x faster than the next-fastest implementation in this
workload. The specialization has a lifecycle tradeoff at tiny sizes: with 1,000 IDs spread across
786 actors, allocating and releasing actor storage makes `uuid_map` insert/remove slower than the
flat hash maps. From the 50,000-ID row it wins all four measured operations.

### Complete tables

Successful read:

| IDs | Actors | uuid_map | ankerl dense | Boost flat | std::unordered_map | std::flat_map |
|---:|---:|---:|---:|---:|---:|---:|
| 1k | 786 | **5.35** | 6.26 | 19.07 | 32.08 | 76.71 |
| 10k | 394 | **12.21** | 20.55 | 16.24 | 71.81 | 156.81 |
| 50k | 290 | **6.82** | 36.11 | 43.31 | 138.13 | 179.64 |
| 100k | 195 | **11.57** | 61.72 | 51.01 | 155.65 | 187.72 |
| 250k | 88 | **8.44** | 80.17 | 109.53 | 152.36 | 163.74 |
| 500k | 218 | **14.93** | 72.57 | 108.97 | 204.71 | 208.41 |
| 1M | 869 | **20.00** | 68.06 | 112.76 | 153.81 | 864.40 |
| 5M | 542 | **12.47** | 69.11 | 169.78 | 324.36 | 945.97 |
| 10M | 464 | **11.60** | 61.89 | 105.48 | 314.10 | 544.75 |
| Mean | - | **11.49** | 52.94 | 81.79 | 171.89 | 369.80 |

Existing write/update:

| IDs | Actors | uuid_map | ankerl dense | Boost flat | std::unordered_map | std::flat_map |
|---:|---:|---:|---:|---:|---:|---:|
| 1k | 786 | 13.75 | **6.81** | 16.30 | 28.70 | 84.02 |
| 10k | 394 | 14.68 | 21.40 | **11.29** | 64.00 | 158.39 |
| 50k | 290 | **15.27** | 43.09 | 28.46 | 131.60 | 184.50 |
| 100k | 195 | **19.07** | 50.51 | 48.38 | 186.27 | 212.28 |
| 250k | 88 | **15.50** | 65.19 | 117.76 | 226.22 | 191.16 |
| 500k | 218 | **21.82** | 87.24 | 130.84 | 253.14 | 372.80 |
| 1M | 869 | **24.06** | 140.17 | 143.50 | 215.56 | 454.60 |
| 5M | 542 | **20.56** | 146.74 | 313.04 | 426.37 | 781.29 |
| 10M | 464 | **20.43** | 149.41 | 201.63 | 378.71 | 586.89 |
| Mean | - | **18.35** | 78.95 | 112.35 | 212.29 | 336.21 |

Empty-to-N autogrow insert:

| IDs | Actors | uuid_map | ankerl dense | Boost flat | std::unordered_map | std::flat_map |
|---:|---:|---:|---:|---:|---:|---:|
| 1k | 786 | 224.70 | **78.10** | 107.20 | 154.40 | 257.50 |
| 10k | 394 | **34.99** | 87.62 | 61.98 | 149.46 | 1985.61 |
| 50k | 290 | **32.08** | 111.09 | 67.70 | 548.43 | n/a |
| 100k | 195 | **35.16** | 107.74 | 59.66 | 544.06 | n/a |
| 250k | 88 | **27.64** | 185.23 | 121.44 | 489.51 | n/a |
| 500k | 218 | **42.65** | 247.82 | 92.65 | 749.89 | n/a |
| 1M | 869 | **39.58** | 258.14 | 152.50 | 841.41 | n/a |
| 5M | 542 | **49.13** | 250.82 | 241.21 | 1724.89 | n/a |
| 10M | 464 | **26.04** | 207.18 | 184.47 | 920.24 | n/a |
| Mean | - | **56.89** | 170.42 | 120.98 | 680.25 | n/a |

Successful remove:

| IDs | Actors | uuid_map | ankerl dense | Boost flat | std::unordered_map | std::flat_map |
|---:|---:|---:|---:|---:|---:|---:|
| 1k | 786 | 236.70 | 19.30 | **19.20** | 84.20 | 169.30 |
| 10k | 394 | 32.16 | 25.20 | **10.20** | 116.94 | 1655.32 |
| 50k | 290 | **24.20** | 48.02 | 25.11 | 335.33 | n/a |
| 100k | 195 | **24.89** | 72.31 | 35.88 | 301.78 | n/a |
| 250k | 88 | **20.86** | 98.08 | 102.35 | 279.16 | n/a |
| 500k | 218 | **27.14** | 133.61 | 134.46 | 427.33 | n/a |
| 1M | 869 | **22.09** | 143.27 | 173.57 | 334.60 | n/a |
| 5M | 542 | **27.18** | 153.57 | 364.63 | 646.81 | n/a |
| 10M | 464 | **18.48** | 137.74 | 207.06 | 528.13 | n/a |
| Mean | - | **48.19** | 92.35 | 119.16 | 339.36 | n/a |

Absolute values vary with CPU frequency, power state, memory, compiler, and background load. Use
numbers from one complete run for comparisons; do not combine the best values from separate runs.

## Run

```sh
cmake -S . -B build/benchmarks -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DUUID_MAP_BUILD_TESTS=OFF \
  -DUUID_MAP_BUILD_BENCHMARKS=ON
cmake --build build/benchmarks --parallel
./build/benchmarks/benchmarks/uuid_map_benchmark
```

The executable prints complete tables for 1k, 10k, 50k, 100k, 250k, 500k, 1M, 5M, and 10M IDs,
plus arithmetic means calculated only when every size has a result. If the active C++23 standard
library does not provide `<flat_map>`, that column is reported as `n/a` while the other four
implementations are still measured.
