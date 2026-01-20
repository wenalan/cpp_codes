#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <map>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include "treap.hpp"

struct BenchmarkResult {
    std::string name;
    std::size_t operations = 0;
    double ms = 0.0;
    double ns_per_op = 0.0;
    std::uint64_t checksum = 0;
};

volatile std::uint64_t g_sink = 0;

std::vector<int> make_unique_keys(std::size_t count) {
    std::vector<int> keys(count);
    std::iota(keys.begin(), keys.end(), 0);
    std::mt19937 rng(std::random_device{}());
    std::shuffle(keys.begin(), keys.end(), rng);
    return keys;
}

std::vector<int> make_queries(const std::vector<int>& keys, std::size_t total_queries) {
    std::vector<int> queries;
    queries.reserve(total_queries);
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<std::size_t> dist(0, keys.size() - 1);

    for (std::size_t i = 0; i < total_queries; ++i) {
        if (i % 2 == 0) {
            queries.push_back(keys[dist(rng)]);
        } else {
            queries.push_back(static_cast<int>(keys.size() + i)); // guaranteed miss
        }
    }
    return queries;
}

template <typename Map, typename Inserter>
BenchmarkResult bench_insert(const std::string& name, Map& map, const std::vector<int>& keys, Inserter inserter) {
    const auto start = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < keys.size(); ++i) {
        inserter(map, keys[i], static_cast<int>(i));
    }
    const auto end = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(end - start).count();
    const double ns_per_op = (ms * 1e6) / static_cast<double>(keys.size());
    g_sink = static_cast<std::uint64_t>(map.size());
    return BenchmarkResult{name, keys.size(), ms, ns_per_op, g_sink};
}

template <typename Map, typename Finder>
BenchmarkResult bench_find(const std::string& name, Map& map, const std::vector<int>& queries, Finder finder) {
    std::uint64_t checksum = 0;
    const auto start = std::chrono::steady_clock::now();
    for (int key : queries) {
        checksum += finder(map, key);
    }
    const auto end = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(end - start).count();
    const double ns_per_op = (ms * 1e6) / static_cast<double>(queries.size());
    g_sink = checksum;
    return BenchmarkResult{name, queries.size(), ms, ns_per_op, checksum};
}

template <typename Map, typename Inserter>
Map preload(const std::vector<int>& keys, Inserter inserter) {
    Map map;
    for (std::size_t i = 0; i < keys.size(); ++i) {
        inserter(map, keys[i], static_cast<int>(i));
    }
    return map;
}

template <typename Map, typename Eraser>
BenchmarkResult bench_erase(const std::string& name, Map& map, const std::vector<int>& erase_order, Eraser eraser) {
    const auto start = std::chrono::steady_clock::now();
    std::size_t removed = 0;
    for (int key : erase_order) {
        removed += eraser(map, key) ? 1 : 0;
    }
    const auto end = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(end - start).count();
    const double ns_per_op = (ms * 1e6) / static_cast<double>(erase_order.size());
    g_sink = static_cast<std::uint64_t>(removed);
    return BenchmarkResult{name, erase_order.size(), ms, ns_per_op, g_sink};
}

void print_result(const BenchmarkResult& r) {
    std::cout << r.name << "\n";
    std::cout << "  operations: " << r.operations << "\n";
    std::cout << "  time (ms): " << r.ms << "\n";
    std::cout << "  ns/op: " << r.ns_per_op << "\n";
    std::cout << "  checksum: " << r.checksum << "\n\n";
}

int main() {
    constexpr std::size_t kKeyCount = 200'000;
    constexpr std::size_t kQueryCount = 300'000;

    auto keys = make_unique_keys(kKeyCount);
    auto queries = make_queries(keys, kQueryCount);
    auto erase_order = keys;

    TreapMap<int, int> treap;
    std::map<int, int> ordered_map;

    auto treap_insert = bench_insert("Treap insert (emplace/assign)", treap, keys,
                                     [](auto& m, int key, int value) { m.insert_or_assign(key, value); });
    auto map_insert = bench_insert("std::map insert (emplace)", ordered_map, keys,
                                   [](auto& m, int key, int value) { m.emplace(key, value); });

    auto treap_find = bench_find("Treap find (50% miss)", treap, queries, [](auto& m, int key) {
        const int* ptr = m.find(key);
        return ptr ? static_cast<std::uint64_t>(*ptr) : 0ULL;
    });

    auto map_find = bench_find("std::map find (50% miss)", ordered_map, queries, [](auto& m, int key) {
        auto it = m.find(key);
        return it == m.end() ? 0ULL : static_cast<std::uint64_t>(it->second);
    });

    auto treap_for_erase =
        preload<TreapMap<int, int>>(keys, [](auto& m, int key, int value) { m.insert_or_assign(key, value); });
    auto map_for_erase = preload<std::map<int, int>>(keys, [](auto& m, int key, int value) { m.emplace(key, value); });

    auto treap_erase = bench_erase("Treap erase", treap_for_erase, erase_order,
                                   [](auto& m, int key) { return m.erase(key); });

    auto map_erase = bench_erase("std::map erase", map_for_erase, erase_order,
                                 [](auto& m, int key) { return m.erase(key) > 0; });

    print_result(treap_insert);
    print_result(map_insert);
    print_result(treap_find);
    print_result(map_find);
    print_result(treap_erase);
    print_result(map_erase);
    return 0;
}
