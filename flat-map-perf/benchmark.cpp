#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <map>
#include <numeric>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#if __has_include(<absl/container/flat_hash_map.h>)
#include <absl/container/flat_hash_map.h>
#define HAS_ABSL_FLAT_HASH_MAP 1
#else
#define HAS_ABSL_FLAT_HASH_MAP 0
#endif

#if __has_include(<tsl/robin_map.h>)
#include <tsl/robin_map.h>
#define HAS_TSL_ROBIN_MAP 1
#else
#define HAS_TSL_ROBIN_MAP 0
#endif

#if __has_include(<tsl/robin_pg_map.h>)
#include <tsl/robin_pg_map.h>
#define HAS_TSL_ROBIN_PG_MAP 1
#else
#define HAS_TSL_ROBIN_PG_MAP 0
#endif

#if __has_include(<ankerl/unordered_dense.h>)
#include <ankerl/unordered_dense.h>
#define HAS_ANKERL_UNORDERED_DENSE 1
#else
#define HAS_ANKERL_UNORDERED_DENSE 0
#endif

#if __has_include(<folly/container/F14Map.h>)
#include <folly/container/F14Map.h>
#define HAS_FOLLY_F14 1
#else
#define HAS_FOLLY_F14 0
#endif

#if __has_include(<flat_map>)
#include <flat_map>
#else
#error "This benchmark requires <flat_map>; build with a standard library that implements C++23 flat_map (e.g., GCC 14+/Clang 17+)."
#endif

namespace {
using Clock = std::chrono::steady_clock;

struct BenchmarkResult {
    std::string name;
    double build_ms;
    double lookup_ms;
    std::size_t checksum;
};

double to_millis(const Clock::time_point start, const Clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

std::vector<int> make_shuffled_keys(std::size_t count, std::mt19937_64& rng) {
    std::vector<int> keys(count);
    std::iota(keys.begin(), keys.end(), 0);
    std::shuffle(keys.begin(), keys.end(), rng);
    return keys;
}

std::vector<int> make_queries(const std::vector<int>& keys, std::size_t count, std::mt19937_64& rng) {
    if (keys.empty() || count == 0) {
        return {};
    }

    std::vector<int> queries;
    queries.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        queries.push_back(keys[i % keys.size()]);
    }
    std::shuffle(queries.begin(), queries.end(), rng);
    return queries;
}

template <typename Map>
BenchmarkResult run_benchmark(std::string_view name, const std::vector<int>& keys, const std::vector<int>& queries) {
    Map map;

    if constexpr (requires(Map& m, std::size_t reserve_size) { m.reserve(reserve_size); }) {
        map.reserve(keys.size());
    }

    const auto build_start = Clock::now();
    for (std::size_t i = 0; i < keys.size(); ++i) {
        map.emplace(keys[i], static_cast<int>(i));
    }
    const auto build_end = Clock::now();

    volatile std::size_t checksum = 0;
    const auto lookup_start = Clock::now();
    for (const int key : queries) {
        auto it = map.find(key);
        if (it != map.end()) {
            checksum += static_cast<std::size_t>(it->second);
        }
    }
    const auto lookup_end = Clock::now();

    return BenchmarkResult{
        std::string{name},
        to_millis(build_start, build_end),
        to_millis(lookup_start, lookup_end),
        checksum,
    };
}

void print_result(const BenchmarkResult& result) {
    std::cout << result.name << "\n";
    std::cout << "  build:  " << result.build_ms << " ms\n";
    std::cout << "  lookup: " << result.lookup_ms << " ms\n";
    std::cout << "  checksum: " << result.checksum << "\n";
}
}  // namespace

int main(int argc, char** argv) {
    const std::size_t count = argc > 1 ? std::stoull(argv[1]) : 200'000;
    const std::size_t lookups = argc > 2 ? std::stoull(argv[2]) : count;
    const std::uint64_t seed = argc > 3 ? std::stoull(argv[3]) : 2024;

    std::mt19937_64 rng(seed);
    const auto keys = make_shuffled_keys(count, rng);
    const auto queries = make_queries(keys, lookups, rng);

    std::cout << "Elements: " << count << ", lookups: " << lookups << ", seed: " << seed << "\n\n";
    std::vector<std::string> skipped;

    std::vector<BenchmarkResult> results;
    results.reserve(10);

    results.push_back(run_benchmark<std::map<int, int>>("std::map", keys, queries));
    results.push_back(run_benchmark<std::flat_map<int, int>>("std::flat_map", keys, queries));
    results.push_back(run_benchmark<std::unordered_map<int, int>>("std::unordered_map", keys, queries));

#if HAS_ABSL_FLAT_HASH_MAP
    results.push_back(run_benchmark<absl::flat_hash_map<int, int>>("absl::flat_hash_map", keys, queries));
#else
    skipped.emplace_back("absl::flat_hash_map (missing <absl/container/flat_hash_map.h>)");
#endif

#if HAS_TSL_ROBIN_MAP
    results.push_back(run_benchmark<tsl::robin_map<int, int>>("tsl::robin_map", keys, queries));
#else
    skipped.emplace_back("tsl::robin_map (missing <tsl/robin_map.h>)");
#endif

#if HAS_TSL_ROBIN_PG_MAP
    results.push_back(run_benchmark<tsl::robin_pg_map<int, int>>("tsl::robin_pg_map", keys, queries));
#else
    skipped.emplace_back("tsl::robin_pg_map (missing <tsl/robin_pg_map.h>)");
#endif

#if HAS_ANKERL_UNORDERED_DENSE
    results.push_back(run_benchmark<ankerl::unordered_dense::map<int, int>>("ankerl::unordered_dense::map", keys, queries));
#else
    skipped.emplace_back("ankerl::unordered_dense::map (missing <ankerl/unordered_dense.h>)");
#endif

#if HAS_FOLLY_F14
    results.push_back(run_benchmark<folly::F14FastMap<int, int>>("folly::F14FastMap", keys, queries));
    results.push_back(run_benchmark<folly::F14ValueMap<int, int>>("folly::F14ValueMap", keys, queries));
#else
    skipped.emplace_back("folly::F14*Map (missing <folly/container/F14Map.h>)");
#endif

    for (const auto& result : results) {
        print_result(result);
    }

    if (!skipped.empty()) {
        std::cout << "\nSkipped:\n";
        for (const auto& entry : skipped) {
            std::cout << "  - " << entry << "\n";
        }
    }

    return 0;
}
