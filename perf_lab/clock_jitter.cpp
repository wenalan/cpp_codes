#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>
#include <time.h>
#include <x86intrin.h>

static inline uint64_t tsc_start() { _mm_lfence(); return __rdtsc(); }
static inline uint64_t tsc_stop()  { unsigned aux; uint64_t t=__rdtscp(&aux); _mm_lfence(); return t; }

static inline void do_clock_gettime() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    asm volatile("" ::: "memory");
}

int main() {
    const int N = 200000;
    std::vector<uint64_t> cyc;
    cyc.reserve(N);

    // warmup
    for (int i=0;i<10000;i++) do_clock_gettime();

    for (int i=0;i<N;i++) {
        uint64_t c0 = tsc_start();
        do_clock_gettime();
        uint64_t c1 = tsc_stop();
        cyc.push_back(c1 - c0);
    }

    std::sort(cyc.begin(), cyc.end());
    auto p = [&](double q){ return cyc[(size_t)(q*(N-1))]; };

    std::cout << "cycles: p50=" << p(0.50) << " p90=" << p(0.90)
              << " p99=" << p(0.99) << " max=" << cyc.back() << "\n";
}

