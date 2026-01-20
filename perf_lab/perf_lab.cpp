// perf_lab.cpp
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <numeric>
#include <random>
#include <string>
#include <thread>
#include <vector>

#if defined(ENABLE_LTTNG)
// Optional LTTng-UST tracepoints (see perf_lab_tp.h below)
#define TRACEPOINT_CREATE_PROBES
#define TRACEPOINT_DEFINE
#include "perf_lab_tp.h"
#else
// No-op when LTTng disabled
#define tracepoint(provider, name, ...) ((void)0)
#endif

#if defined(__GNUC__) || defined(__clang__)
#define NOINLINE __attribute__((noinline))
#else
#define NOINLINE
#endif

static std::atomic<std::uint64_t> g_sink{0};

static inline std::uint64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

struct Args {
    std::string mode = "rowcol";   // rowcol|ptr|branch|false_share|lock|malloc|syscall|fault|all
    std::string variant = "both";  // bad|good|both
    std::size_t size = 8192;       // generic size (e.g. matrix N, elements, pages, etc.)
    std::size_t iters = 50;        // loop iterations
    int threads = 2;               // thread count
    std::size_t chunk = 64;        // bytes per write in syscall mode
};

static void usage(const char* prog) {
    std::cerr
        << "Usage: " << prog << " [--mode=...] [--variant=bad|good|both]\n"
        << "                 [--size=N] [--iters=N] [--threads=N] [--chunk=BYTES]\n"
        << "Modes: rowcol, ptr, branch, false_share, lock, malloc, syscall, fault, all\n";
}

static Args parse_args(int argc, char** argv) {
    Args a;
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
        } else if (auto v = get("--mode")) {
            a.mode = v;
        } else if (auto v = get("--variant")) {
            a.variant = v;
        } else if (auto v = get("--size")) {
            a.size = std::stoull(v);
        } else if (auto v = get("--iters")) {
            a.iters = std::stoull(v);
        } else if (auto v = get("--threads")) {
            a.threads = std::stoi(v);
        } else if (auto v = get("--chunk")) {
            a.chunk = std::stoull(v);
        } else {
            std::cerr << "Unknown arg: " << s << "\n";
            usage(argv[0]);
            std::exit(1);
        }
    }
    if (a.threads <= 0) a.threads = 1;
    return a;
}

template <class Fn>
static std::uint64_t time_run(const std::string& mode, const std::string& variant, Fn&& fn) {
    tracepoint(perf_lab, phase_begin, mode.c_str(), variant.c_str(), (std::uint64_t)now_ns());
    auto t0 = std::chrono::steady_clock::now();
    std::uint64_t cs = fn();
    auto t1 = std::chrono::steady_clock::now();
    tracepoint(perf_lab, phase_end, mode.c_str(), variant.c_str(), (std::uint64_t)now_ns());

    g_sink.fetch_add(cs, std::memory_order_relaxed);

    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    return (std::uint64_t)ns;
}

static void print_result(const std::string& mode, const std::string& variant,
                         std::uint64_t ns, std::uint64_t work_hint) {
    double ms = (double)ns / 1e6;
    double ns_per = work_hint ? (double)ns / (double)work_hint : 0.0;
    std::cout << std::left << std::setw(12) << mode
              << std::setw(8) << variant
              << "  " << std::setw(10) << std::fixed << std::setprecision(3) << ms << " ms";
    if (work_hint) {
        std::cout << "  (" << std::fixed << std::setprecision(2) << ns_per << " ns/op)";
    }
    std::cout << "\n";
}

// -------------------- mode: rowcol --------------------
// Matrix N x N, row-major contiguous memory.
// bad: col-major traversal
// good: row-major traversal
NOINLINE static std::uint64_t row_major(std::uint32_t* a, std::size_t N, std::size_t iters) {
    std::uint64_t sum = 0;
    for (std::size_t it = 0; it < iters; ++it) {
        for (std::size_t i = 0; i < N; ++i) {
            std::size_t base = i * N;
            for (std::size_t j = 0; j < N; ++j) {
                sum += a[base + j];
            }
        }
    }
    return sum;
}

NOINLINE static std::uint64_t col_major(std::uint32_t* a, std::size_t N, std::size_t iters) {
    std::uint64_t sum = 0;
    for (std::size_t it = 0; it < iters; ++it) {
        for (std::size_t j = 0; j < N; ++j) {
            for (std::size_t i = 0; i < N; ++i) {
                sum += a[i * N + j];
            }
        }
    }
    return sum;
}

static void run_rowcol(const Args& a) {
    std::size_t N = a.size;
    std::size_t elems = N * N;
    std::vector<std::uint32_t> mat(elems);
    std::iota(mat.begin(), mat.end(), 1);

    auto do_good = [&]() {
        std::uint64_t ns = time_run("rowcol", "good", [&] { return row_major(mat.data(), N, a.iters); });
        print_result("rowcol", "good", ns, elems * a.iters);
    };
    auto do_bad = [&]() {
        std::uint64_t ns = time_run("rowcol", "bad",  [&] { return col_major(mat.data(), N, a.iters); });
        print_result("rowcol", "bad",  ns, elems * a.iters);
    };

    if (a.variant == "good") do_good();
    else if (a.variant == "bad") do_bad();
    else { do_good(); do_bad(); }
}

// -------------------- mode: ptr (pointer chasing) --------------------
// Random permutation next[], then chase.
NOINLINE static std::uint64_t ptr_chase(const std::uint32_t* next, std::size_t n, std::size_t steps) {
    std::uint32_t idx = 0;
    std::uint64_t acc = 0;
    for (std::size_t i = 0; i < steps; ++i) {
        idx = next[idx];
        acc += idx;
    }
    return acc;
}

static void run_ptr(const Args& a) {
    std::size_t n = a.size;
    std::vector<std::uint32_t> next(n);
    std::vector<std::uint32_t> perm(n);
    std::iota(perm.begin(), perm.end(), 0);

    std::mt19937_64 rng(12345);
    std::shuffle(perm.begin(), perm.end(), rng);

    for (std::size_t i = 0; i + 1 < n; ++i) next[perm[i]] = perm[i + 1];
    next[perm[n - 1]] = perm[0];

    // good: sequential array walk (prefetch-friendly)
    auto seq_walk = [&]() -> std::uint64_t {
        std::uint64_t acc = 0;
        for (std::size_t it = 0; it < a.iters; ++it) {
            for (std::size_t i = 0; i < n; ++i) acc += next[i];
        }
        return acc;
    };

    // bad: random pointer chasing (latency bound)
    auto random_chase = [&]() -> std::uint64_t {
        std::size_t steps = n * a.iters;
        return ptr_chase(next.data(), n, steps);
    };

    auto do_good = [&]() {
        std::uint64_t ns = time_run("ptr", "good", seq_walk);
        print_result("ptr", "good", ns, n * a.iters);
    };
    auto do_bad = [&]() {
        std::uint64_t ns = time_run("ptr", "bad", random_chase);
        print_result("ptr", "bad", ns, n * a.iters);
    };

    if (a.variant == "good") do_good();
    else if (a.variant == "bad") do_bad();
    else { do_good(); do_bad(); }
}

// -------------------- mode: branch --------------------
// good: predictable branch
// bad: unpredictable branch
NOINLINE static std::uint64_t branch_predictable(std::uint32_t* x, std::size_t n, std::size_t iters) {
    std::uint64_t acc = 0;
    for (std::size_t it = 0; it < iters; ++it) {
        for (std::size_t i = 0; i < n; ++i) {
            if ((i & 1023) == 0) acc += x[i];
            else acc += 1;
        }
    }
    return acc;
}

NOINLINE static std::uint64_t branch_unpredictable(std::uint32_t* x, std::size_t n, std::size_t iters) {
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 1);
    std::uint64_t acc = 0;
    for (std::size_t it = 0; it < iters; ++it) {
        for (std::size_t i = 0; i < n; ++i) {
            if (dist(rng)) acc += x[i];
            else acc += 1;
        }
    }
    return acc;
}

static void run_branch(const Args& a) {
    std::size_t n = a.size;
    std::vector<std::uint32_t> x(n);
    std::iota(x.begin(), x.end(), 1);

    auto do_good = [&]() {
        std::uint64_t ns = time_run("branch", "good", [&] { return branch_predictable(x.data(), n, a.iters); });
        print_result("branch", "good", ns, n * a.iters);
    };
    auto do_bad = [&]() {
        std::uint64_t ns = time_run("branch", "bad",  [&] { return branch_unpredictable(x.data(), n, a.iters); });
        print_result("branch", "bad",  ns, n * a.iters);
    };

    if (a.variant == "good") do_good();
    else if (a.variant == "bad") do_bad();
    else { do_good(); do_bad(); }
}

// -------------------- mode: false_share --------------------
// bad: two atomics adjacent -> same cache line bouncing
// good: padded to separate cache lines
struct BadCounters {
    std::atomic<std::uint64_t> a{0};
    std::atomic<std::uint64_t> b{0};
};

struct GoodCounters {
    alignas(64) std::atomic<std::uint64_t> a{0};
    alignas(64) std::atomic<std::uint64_t> b{0};
};

template <typename C>
static std::uint64_t false_share_run(C& c, std::size_t iters) {
    auto workerA = [&]() {
        for (std::size_t i = 0; i < iters; ++i) c.a.fetch_add(1, std::memory_order_relaxed);
    };
    auto workerB = [&]() {
        for (std::size_t i = 0; i < iters; ++i) c.b.fetch_add(1, std::memory_order_relaxed);
    };
    std::thread t1(workerA), t2(workerB);
    t1.join(); t2.join();
    return c.a.load(std::memory_order_relaxed) + c.b.load(std::memory_order_relaxed);
}

static void run_false_share(const Args& a) {
    std::size_t iters = a.size * a.iters;

    auto do_good = [&]() {
        GoodCounters c;
        std::uint64_t ns = time_run("false_share", "good", [&] { return false_share_run(c, iters); });
        print_result("false_share", "good", ns, iters * 2);
    };
    auto do_bad = [&]() {
        BadCounters c;
        std::uint64_t ns = time_run("false_share", "bad",  [&] { return false_share_run(c, iters); });
        print_result("false_share", "bad",  ns, iters * 2);
    };

    if (a.variant == "good") do_good();
    else if (a.variant == "bad") do_bad();
    else { do_good(); do_bad(); }
}

// -------------------- mode: lock --------------------
// bad: one mutex, all threads contend
// good: per-thread local counters then sum
static void run_lock(const Args& a) {
    int T = a.threads;
    std::size_t iters = a.size * a.iters;

    auto bad = [&]() -> std::uint64_t {
        std::mutex m;
        std::uint64_t shared = 0;
        auto worker = [&]() {
            for (std::size_t i = 0; i < iters; ++i) {
                std::lock_guard<std::mutex> lk(m);
                shared++;
            }
        };
        std::vector<std::thread> ts;
        ts.reserve(T);
        for (int i = 0; i < T; ++i) ts.emplace_back(worker);
        for (auto& t : ts) t.join();
        return shared;
    };

    auto good = [&]() -> std::uint64_t {
        std::vector<std::uint64_t> locals((std::size_t)T, 0);
        auto worker = [&](int tid) {
            std::uint64_t x = 0;
            for (std::size_t i = 0; i < iters; ++i) x++;
            locals[(std::size_t)tid] = x;
        };
        std::vector<std::thread> ts;
        ts.reserve(T);
        for (int i = 0; i < T; ++i) ts.emplace_back(worker, i);
        for (auto& t : ts) t.join();
        return std::accumulate(locals.begin(), locals.end(), (std::uint64_t)0);
    };

    auto do_good = [&]() {
        std::uint64_t ns = time_run("lock", "good", good);
        print_result("lock", "good", ns, iters * (std::size_t)T);
    };
    auto do_bad = [&]() {
        std::uint64_t ns = time_run("lock", "bad", bad);
        print_result("lock", "bad", ns, iters * (std::size_t)T);
    };

    if (a.variant == "good") do_good();
    else if (a.variant == "bad") do_bad();
    else { do_good(); do_bad(); }
}

// -------------------- mode: malloc --------------------
// bad: new/delete many small objects
// good: reuse a pool
struct Node {
    std::uint64_t x, y;
};

static void run_malloc(const Args& a) {
    std::size_t n = a.size;
    std::size_t iters = a.iters;

    auto bad = [&]() -> std::uint64_t {
        std::uint64_t acc = 0;
        for (std::size_t it = 0; it < iters; ++it) {
            for (std::size_t i = 0; i < n; ++i) {
                Node* p = new Node{ i, it };
                acc += p->x + p->y;
                delete p;
            }
        }
        return acc;
    };

    auto good = [&]() -> std::uint64_t {
        std::vector<Node> pool(n);
        std::uint64_t acc = 0;
        for (std::size_t it = 0; it < iters; ++it) {
            for (std::size_t i = 0; i < n; ++i) {
                pool[i] = Node{ i, it };
                acc += pool[i].x + pool[i].y;
            }
        }
        return acc;
    };

    auto do_good = [&]() {
        std::uint64_t ns = time_run("malloc", "good", good);
        print_result("malloc", "good", ns, n * iters);
    };
    auto do_bad = [&]() {
        std::uint64_t ns = time_run("malloc", "bad", bad);
        print_result("malloc", "bad", ns, n * iters);
    };

    if (a.variant == "good") do_good();
    else if (a.variant == "bad") do_bad();
    else { do_good(); do_bad(); }
}

// -------------------- mode: syscall --------------------
// bad: many small writes
// good: fewer larger writes (buffered)
static void run_syscall(const Args& a) {
    std::size_t total = a.size * a.iters; // total bytes
    std::size_t small = a.chunk;
    if (small == 0) small = 64;

    auto bad = [&]() -> std::uint64_t {
        std::ofstream out("/dev/null", std::ios::binary);
        std::vector<char> buf(small, 'x');
        std::uint64_t acc = 0;
        for (std::size_t sent = 0; sent < total; sent += small) {
            out.write(buf.data(), (std::streamsize)buf.size());
            acc += (std::uint64_t)buf[0];
        }
        return acc;
    };

    auto good = [&]() -> std::uint64_t {
        std::ofstream out("/dev/null", std::ios::binary);
        std::size_t big = 1 << 20; // 1MiB
        std::vector<char> buf(big, 'x');
        std::uint64_t acc = 0;
        for (std::size_t sent = 0; sent < total; sent += big) {
            std::size_t n = std::min(big, total - sent);
            out.write(buf.data(), (std::streamsize)n);
            acc += (std::uint64_t)buf[0];
        }
        return acc;
    };

    auto do_good = [&]() {
        std::uint64_t ns = time_run("syscall", "good", good);
        print_result("syscall", "good", ns, total);
    };
    auto do_bad = [&]() {
        std::uint64_t ns = time_run("syscall", "bad", bad);
        print_result("syscall", "bad", ns, total);
    };

    if (a.variant == "good") do_good();
    else if (a.variant == "bad") do_bad();
    else { do_good(); do_bad(); }
}

// -------------------- mode: fault --------------------
// Touch many pages; bad: random page touch; good: sequential page touch
static void run_fault(const Args& a) {
    const std::size_t page = 4096;
    std::size_t pages = a.size;
    std::size_t bytes = pages * page;
    std::vector<std::uint8_t> mem(bytes, 0);

    std::vector<std::size_t> idx(pages);
    std::iota(idx.begin(), idx.end(), 0);
    std::mt19937_64 rng(7);
    std::shuffle(idx.begin(), idx.end(), rng);

    auto good = [&]() -> std::uint64_t {
        std::uint64_t acc = 0;
        for (std::size_t it = 0; it < a.iters; ++it) {
            for (std::size_t p = 0; p < pages; ++p) {
                std::size_t off = p * page;
                mem[off] += 1;
                acc += mem[off];
            }
        }
        return acc;
    };

    auto bad = [&]() -> std::uint64_t {
        std::uint64_t acc = 0;
        for (std::size_t it = 0; it < a.iters; ++it) {
            for (std::size_t k = 0; k < pages; ++k) {
                std::size_t p = idx[k];
                std::size_t off = p * page;
                mem[off] += 1;
                acc += mem[off];
            }
        }
        return acc;
    };

    auto do_good = [&]() {
        std::uint64_t ns = time_run("fault", "good", good);
        print_result("fault", "good", ns, pages * a.iters);
    };
    auto do_bad = [&]() {
        std::uint64_t ns = time_run("fault", "bad", bad);
        print_result("fault", "bad", ns, pages * a.iters);
    };

    if (a.variant == "good") do_good();
    else if (a.variant == "bad") do_bad();
    else { do_good(); do_bad(); }
}

static void run_one(const Args& a) {
    if (a.mode == "rowcol") run_rowcol(a);
    else if (a.mode == "ptr") run_ptr(a);
    else if (a.mode == "branch") run_branch(a);
    else if (a.mode == "false_share") run_false_share(a);
    else if (a.mode == "lock") run_lock(a);
    else if (a.mode == "malloc") run_malloc(a);
    else if (a.mode == "syscall") run_syscall(a);
    else if (a.mode == "fault") run_fault(a);
    else if (a.mode == "all") {
        run_rowcol(a);
        run_ptr(a);
        run_branch(a);
        run_false_share(a);
        run_lock(a);
        run_malloc(a);
        run_syscall(a);
        run_fault(a);
    } else {
        std::cerr << "Unknown mode: " << a.mode << "\n";
        usage("perf_lab");
        std::exit(1);
    }
}

int main(int argc, char** argv) {
    Args a = parse_args(argc, argv);
    std::cout << "perf_lab: mode=" << a.mode << " variant=" << a.variant
              << " size=" << a.size << " iters=" << a.iters
              << " threads=" << a.threads << " chunk=" << a.chunk << "\n";
    run_one(a);
    std::cout << "sink=" << g_sink.load(std::memory_order_relaxed) << "\n";
    return 0;
}

