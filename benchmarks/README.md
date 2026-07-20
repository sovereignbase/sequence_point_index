# Benchmarks

The benchmark measures the fixed sequence-point registry directly. Every address consists of a
random 96-bit actor ID and a dense 32-bit actor-local index. Only actors are hashed; reads resolve
an actor pointer and then access `values[local_index]`.

The actor counts and 15 data sizes reproduce the reference workload from 1 through 10 million
IDs. Actor selection is random and interleaved while each actor's local sequence increases
monotonically. This preserves the locality inherent in an actor sequence without sorting queries.
A fully shuffled 40 MiB value array measures a different, random-memory workload.

The reported columns are:

- successful `read`
- pre-resolved `values[index]`, the locality lower bound
- existing-address `write`
- successful `remove`, including automatic release and shrink work

Each read and write result is the median of five samples with at least ten million operations per
sample. Remove processes every address once per sample because it mutates the whole registry. The
Release benchmark target uses `-march=native`; the library itself does not impose architecture
flags.

```sh
cmake -S . -B build/benchmarks -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DUUID_MAP_BUILD_TESTS=OFF \
  -DUUID_MAP_BUILD_BENCHMARKS=ON
cmake --build build/benchmarks
./build/benchmarks/benchmarks/uuid_map_benchmark
```

Results depend on CPU frequency, compiler, power state, memory, and background load. Compare runs
made on the same machine under the same conditions.

## Current reference run

Intel Core i5-10210U, Windows x64, Release with `-O3 -DNDEBUG -march=native`:

| Compiler | Read mean, 15 sizes | 10M read | 10M resolved | 10M write | 10M remove |
|---|---:|---:|---:|---:|---:|
| GCC 16.1.0 | 4.53 ns | 7.87 ns | 3.93 ns | 9.20 ns | 11.51 ns |
| Clang 22.1.6 | 3.80 ns | 7.51 ns | 3.87 ns | 11.26 ns | 10.82 ns |

Every number is nanoseconds per operation. These are throughput measurements for the documented
actor-interleaved sequence workload, not a promise of identical latency on other hardware.
