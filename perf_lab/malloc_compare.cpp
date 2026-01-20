// Benchmark glibc malloc vs jemalloc/tcmalloc using per-thread churn workloads.
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <iomanip>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if defined(__GNUC__) || defined(__clang__)
#define NOINLINE __attribute__((noinline))
#else
#define NOINLINE
#endif

using MallocFn = void* (*)(std::size_t);
using FreeFn = void (*)(void*);

struct Allocator {
    std::string name;
    void* handle = nullptr;  // dlopen handle (null for glibc)
    MallocFn malloc_fn = nullptr;
    FreeFn free_fn = nullptr;

    Allocator() = default;
    Allocator(const Allocator&) = delete;
    Allocator& operator=(const Allocator&) = delete;

    Allocator(Allocator&& other) noexcept
        : name(std::move(other.name)),
          handle(other.handle),
          malloc_fn(other.malloc_fn),
          free_fn(other.free_fn) {
        other.handle = nullptr;
        other.malloc_fn = nullptr;
        other.free_fn = nullptr;
    }

    Allocator& operator=(Allocator&& other) noexcept {
        if (this != &other) {
            if (handle) dlclose(handle);
            name = std::move(other.name);
            handle = other.handle;
            malloc_fn = other.malloc_fn;
            free_fn = other.free_fn;
            other.handle = nullptr;
            other.malloc_fn = nullptr;
            other.free_fn = nullptr;
        }
        return *this;
    }

    ~Allocator() {
        if (handle) dlclose(handle);
    }
};

struct Options {
    std::size_t allocs_per_thread = 2'000'000;
    std::size_t live_slots = 4096;
    std::size_t small_max = 4096;   // upper bound for "small" allocs
    std::size_t large_size = 65536; // fixed large alloc size
    double large_ratio = 0.10;      // fraction of allocs that are large
    std::size_t batch = 128;        // allocs before doing a free batch
    std::string mode = "glibc";     // label: glibc|jemalloc|tcmalloc (allocator chosen via LD_PRELOAD)
    int threads = std::max(1u, std::thread::hardware_concurrency());
    int warmup = 1;
    int reps = 3;
    bool verbose = false;
};

static void usage(const char* prog) {
    std::cerr << "Usage: " << prog
              << " [--allocs=N] [--live=N] [--small-max=N] [--large-size=N]\n"
                 "            [--large-ratio=FLOAT] [--batch=N] [--threads=N]\n"
                 "            [--warmup=N] [--reps=N] [--verbose]\n"
                 "            [--mode=glibc|jemalloc|tcmalloc]\n"
                 "Build notes:\n"
                 "  g++ -O2 -std=c++17 malloc_compare.cpp -ldl -pthread\n"
                 "  Compare allocators by running separate processes, e.g.:\n"
                 "    ./malloc_compare --mode=glibc ...\n"
                 "    LD_PRELOAD=libjemalloc.so.2 ./malloc_compare --mode=jemalloc ...\n"
                 "    LD_PRELOAD=libtcmalloc.so.4 ./malloc_compare --mode=tcmalloc ...\n";
}

static Options parse_args(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        std::string s(argv[i]);
        auto get = [&](const char* key) -> const char* {
            std::size_t klen = std::strlen(key);
            if (s.rfind(key, 0) == 0 && s.size() > klen && s[klen] == '=') {
                return s.c_str() + klen + 1;
            }
            return nullptr;
        };

        if (s == "-h" || s == "--help") {
            usage(argv[0]);
            std::exit(0);
        } else if (auto v = get("--allocs")) {
            opt.allocs_per_thread = std::stoull(v);
        } else if (auto v = get("--live")) {
            opt.live_slots = std::stoull(v);
        } else if (auto v = get("--small-max")) {
            opt.small_max = std::stoull(v);
        } else if (auto v = get("--large-size")) {
            opt.large_size = std::stoull(v);
        } else if (auto v = get("--large-ratio")) {
            opt.large_ratio = std::stod(v);
        } else if (auto v = get("--batch")) {
            opt.batch = std::stoull(v);
        } else if (auto v = get("--mode")) {
            opt.mode = v;
        } else if (auto v = get("--threads")) {
            opt.threads = std::stoi(v);
        } else if (auto v = get("--warmup")) {
            opt.warmup = std::stoi(v);
        } else if (auto v = get("--reps")) {
            opt.reps = std::stoi(v);
        } else if (s == "--verbose") {
            opt.verbose = true;
        } else {
            std::cerr << "Unknown arg: " << s << "\n";
            usage(argv[0]);
            std::exit(1);
        }
    }
    if (opt.threads <= 0) opt.threads = 1;
    if (opt.live_slots == 0) opt.live_slots = 1024;
    if (opt.small_max < 16) opt.small_max = 16;
    if (opt.large_size < opt.small_max) opt.large_size = opt.small_max;
    if (opt.large_ratio < 0.0) opt.large_ratio = 0.0;
    if (opt.large_ratio > 1.0) opt.large_ratio = 1.0;
    if (opt.batch == 0) opt.batch = 64;
    if (opt.reps <= 0) opt.reps = 1;
    if (opt.warmup < 0) opt.warmup = 0;
    if (opt.mode != "glibc" && opt.mode != "jemalloc" && opt.mode != "tcmalloc") {
        std::cerr << "Unknown mode: " << opt.mode << "\n";
        usage(argv[0]);
        std::exit(1);
    }
    return opt;
}

static Allocator builtin_glibc() {
    Allocator a;
    a.name = "default";
    a.malloc_fn = std::malloc;
    a.free_fn = std::free;
    return a;
}

struct IterResult {
    std::uint64_t ns = 0;
    std::uint64_t ops = 0;
};

NOINLINE static void touch(void* p, std::size_t sz) {
    if (!p || sz == 0) return;
    // Touch enough bytes to fault in pages; stride by 4K.
    constexpr std::size_t stride = 4096;
    std::size_t limit = sz;
    if (limit > stride) limit = stride * ((sz + stride - 1) / stride);  // round up to touch each page start
    for (std::size_t off = 0; off < limit; off += stride) {
        std::memset(static_cast<char*>(p) + off, 0xA5, std::min<std::size_t>(64, sz - off));
    }
}

static IterResult run_once(const Allocator& alloc, const Options& opt, int seed_offset) {
    using namespace std::chrono;
    std::vector<std::thread> threads;
    threads.reserve(opt.threads);

    struct ThreadSeq {
        std::vector<std::size_t> alloc_sizes;
        std::vector<std::uint64_t> free_picks;
    };

    // Precompute size/free-choice sequences to keep RNG cost out of the timed region.
    std::vector<ThreadSeq> seqs(opt.threads);
    for (int t = 0; t < opt.threads; ++t) {
        seqs[t].alloc_sizes.resize(opt.allocs_per_thread);
        seqs[t].free_picks.resize(opt.allocs_per_thread + opt.live_slots + opt.batch);
        std::mt19937_64 rng(123456789ULL + (std::uint64_t)seed_offset * 101 + (std::uint64_t)t);
        std::uniform_real_distribution<double> coin(0.0, 1.0);
        std::uniform_int_distribution<std::size_t> small_dist(16, opt.small_max);
        for (std::size_t i = 0; i < opt.allocs_per_thread; ++i) {
            bool use_large = coin(rng) < opt.large_ratio;
            seqs[t].alloc_sizes[i] = use_large ? opt.large_size : small_dist(rng);
        }
        for (std::size_t i = 0; i < seqs[t].free_picks.size(); ++i) {
            seqs[t].free_picks[i] = rng();
        }
    }

    auto t0 = steady_clock::now();
    for (int t = 0; t < opt.threads; ++t) {
        threads.emplace_back([&, t] {
            std::vector<void*> slots(opt.live_slots, nullptr);
            std::vector<void*> live;
            live.reserve(opt.live_slots * 2);
            std::size_t free_pick_idx = 0;

            // Initial fill to target live set size.
            std::size_t alloc_idx = 0;
            std::size_t warm = std::min(opt.live_slots, opt.allocs_per_thread);
            for (; alloc_idx < warm; ++alloc_idx) {
                std::size_t sz = seqs[t].alloc_sizes[alloc_idx];
                void* p = alloc.malloc_fn(sz);
                if (!p) {
                    std::cerr << "Allocation failure on " << alloc.name << "\n";
                    std::abort();
                }
                touch(p, sz);
                slots[alloc_idx % opt.live_slots] = p;
                live.push_back(p);
            }

            // Batch allocate, then free random live entries to create churn/fragmentation.
            while (alloc_idx < opt.allocs_per_thread) {
                std::size_t batch = std::min<std::size_t>(opt.batch, opt.allocs_per_thread - alloc_idx);
                for (std::size_t i = 0; i < batch; ++i, ++alloc_idx) {
                    std::size_t sz = seqs[t].alloc_sizes[alloc_idx];
                    void* p = alloc.malloc_fn(sz);
                    if (!p) {
                        std::cerr << "Allocation failure on " << alloc.name << "\n";
                        std::abort();
                    }
                    touch(p, sz);
                    live.push_back(p);
                }

                std::size_t frees = std::min(batch, live.size());
                for (std::size_t i = 0; i < frees; ++i) {
                    if (live.empty()) break;
                    std::size_t pick = seqs[t].free_picks[free_pick_idx++ % seqs[t].free_picks.size()];
                    std::size_t idx = pick % live.size();
                    alloc.free_fn(live[idx]);
                    live[idx] = live.back();
                    live.pop_back();
                }
            }

            for (void* p : live) {
                alloc.free_fn(p);
            }
        });
    }
    for (auto& th : threads) th.join();
    auto t1 = steady_clock::now();

    IterResult r;
    r.ns = (std::uint64_t)duration_cast<nanoseconds>(t1 - t0).count();
    r.ops = opt.allocs_per_thread * (std::uint64_t)opt.threads;
    return r;
}

struct Summary {
    std::string name;
    double best_ms = 0.0;
    double ns_per_op = 0.0;
};

static Summary benchmark(const Allocator& alloc, const Options& opt) {
    IterResult best{~0ULL, 0};

    for (int i = 0; i < opt.warmup; ++i) {
        run_once(alloc, opt, -1000 - i);  // ignore result
    }
    for (int i = 0; i < opt.reps; ++i) {
        IterResult r = run_once(alloc, opt, i);
        if (opt.verbose) {
            double ms = (double)r.ns / 1e6;
            double ns_per = r.ops ? (double)r.ns / (double)r.ops : 0.0;
            std::cout << alloc.name << " iter " << i << ": " << std::fixed << std::setprecision(3)
                      << ms << " ms  (" << ns_per << " ns/op)\n";
        }
        if (r.ns < best.ns) best = r;
    }

    Summary s;
    s.name = alloc.name;
    s.best_ms = (double)best.ns / 1e6;
    s.ns_per_op = best.ops ? (double)best.ns / (double)best.ops : 0.0;
    return s;
}

int main(int argc, char** argv) {
    Options opt = parse_args(argc, argv);

    Summary s = benchmark(builtin_glibc(), opt);
    s.name = opt.mode;  // label reflects intended allocator via LD_PRELOAD

    std::cout << std::left << std::setw(12) << "allocator"
              << std::setw(12) << "best_ms"
              << std::setw(12) << "ns/op" << "\n";
    std::cout << std::left << std::setw(12) << s.name
              << std::setw(12) << std::fixed << std::setprecision(3) << s.best_ms
              << std::setw(12) << std::fixed << std::setprecision(3) << s.ns_per_op
              << "\n";

    return 0;
}
