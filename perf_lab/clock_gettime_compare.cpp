#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

// Direct syscall version.
int my_clock_gettime(clockid_t clk, struct timespec* ts) {
    return syscall(SYS_clock_gettime, clk, ts);
}

static volatile std::uint64_t g_sink = 0;

template <auto Fn>
std::uint64_t bench_ct(const char* label, clockid_t clk, std::uint64_t iters) {
    struct timespec ts{};

    // Warm up so vDSO/glibc setup costs don't skew results.
    for (int i = 0; i < 1000; ++i) {
        Fn(clk, &ts);
    }

    auto t0 = std::chrono::steady_clock::now();
    for (std::uint64_t i = 0; i < iters; ++i) {
        if (Fn(clk, &ts) != 0) {
            std::perror(label);
            break;
        }
        g_sink += static_cast<std::uint64_t>(ts.tv_nsec);
    }
    auto t1 = std::chrono::steady_clock::now();

    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    double ns_per = iters ? static_cast<double>(ns) / static_cast<double>(iters) : 0.0;
    std::cout << label << ": " << ns_per << " ns/call  (" << ns / 1e6 << " ms total)\n";
    return static_cast<std::uint64_t>(ns);
}

int main(int argc, char** argv) {
    std::uint64_t iters = 10'000'000;
    if (argc > 1) {
        iters = std::strtoull(argv[1], nullptr, 0);
        if (iters == 0) iters = 1;
    }

    clockid_t clk = CLOCK_MONOTONIC;
    std::cout << "clock id: " << clk << ", iterations: " << iters << "\n";

    bench_ct<::clock_gettime>("glibc clock_gettime (likely vDSO)", clk, iters);
    bench_ct<my_clock_gettime>("syscall clock_gettime", clk, iters);

    // Prevent dead-code elimination.
    if (g_sink == 0xdeadbeef) std::cout << g_sink << "\n";

    //std::fflush(stdout);
    return 0;
}
