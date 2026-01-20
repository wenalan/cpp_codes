# Flat Map vs Map vs Unordered Map

Simple micro-benchmark comparing `std::flat_map`, `std::map`, `std::unordered_map`, and a handful of popular third-party hash maps (Abseil, Tessil robin_map/robin_pg_map, ankerl::unordered_dense, Folly F14).

## Build

Requires a standard library that provides `std::flat_map` (C++23), e.g. GCC 14+ or Clang 17+. Build with optimization enabled:

```bash
g++ -std=c++23 -O3 flat-map-perf/benchmark.cpp -o flat-map-perf/flat-map-perf
```

Optional maps are compiled only when their headers are found:

- Abseil: `absl/container/flat_hash_map.h`
- Tessil: `tsl/robin_map.h` and/or `tsl/robin_pg_map.h` (header-only)
- ankerl: `ankerl/unordered_dense.h` (header-only)
- Folly: `folly/container/F14Map.h` (requires linking Folly and its dependencies)

Adjust include paths and link flags as needed, e.g. `-I/path/to/headers -lfolly -lglog -ldouble-conversion -lboost_context ...`.

## Run

```
./flat-map-perf/flat-map-perf [element_count] [lookup_count] [seed]
```

Defaults: `element_count=200000`, `lookup_count=element_count`, `seed=2024`.
