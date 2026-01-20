#include <iostream>
#include <cstdint>
#include <unistd.h>
#include <sched.h>

#include <cstdint>
#include <time.h>
#include <x86intrin.h>
#include <sched.h>

static inline uint64_t tsc_start_lfence_rdtsc() {
    unsigned lo, hi;
    asm volatile(
        "lfence\n\t"
        "rdtsc\n\t"
        : "=a"(lo), "=d"(hi)
        :
        : "memory"
    );
    return (uint64_t(hi) << 32) | lo;
}

static inline uint64_t tsc_stop_rdtscp_lfence(unsigned* aux_out = nullptr) {
    unsigned lo, hi, aux;
    asm volatile(
        "rdtscp\n\t"
        "lfence\n\t"
        : "=a"(lo), "=d"(hi), "=c"(aux)
        :
        : "memory"
    );

    if (aux_out) *aux_out = aux;
    return (uint64_t(hi) << 32) | lo;
}

static inline uint64_t nsec_now() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return uint64_t(ts.tv_sec) * 1000000000ull + uint64_t(ts.tv_nsec);
}

// 校准：返回 tsc_hz（ticks per second）
static double calibrate_tsc_hz(uint64_t target_ns = 300000000ull /*300ms*/) {
    // 建议绑核，减少迁核噪声（可选）
    // cpu_set_t set; CPU_ZERO(&set); CPU_SET(0, &set); sched_setaffinity(0, sizeof(set), &set);

    uint64_t t0 = nsec_now();
    uint64_t c0 = tsc_start_lfence_rdtsc();

    // 等到经过 target_ns（不要 sleep，sleep 抖动大；这里是忙等）
    while (nsec_now() - t0 < target_ns) { }

    uint64_t c1 = tsc_stop_rdtscp_lfence();
    uint64_t t1 = nsec_now();

    uint64_t dt_ns = t1 - t0;
    uint64_t dc = c1 - c0;

    return (double)dc * 1e9 / (double)dt_ns; // ticks per second
}

// cycles -> ns
static inline uint64_t cycles_to_ns(uint64_t cycles, double tsc_hz) {
    // ns = cycles * 1e9 / hz
    return (uint64_t)((long double)cycles * 1e9L / (long double)tsc_hz);
}

int main() {
double tsc_hz = calibrate_tsc_hz();   // 例如 ≈ 2.9e9

uint64_t t0 = tsc_start_lfence_rdtsc();
sleep(1);
unsigned aux;
uint64_t t1 = tsc_stop_rdtscp_lfence(&aux);
uint64_t cycles = t1 - t0;

uint64_t ns = cycles_to_ns(cycles, tsc_hz);
std::cout << "cyc " << cycles << " ns " << ns << " aux " << aux << std::endl;
std::cout << "sched_getcpu()=" << sched_getcpu() << std::endl;
}
