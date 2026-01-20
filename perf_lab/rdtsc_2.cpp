#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>
#include <time.h>
#include <sched.h>
#include <x86intrin.h>

// -------------------- TSC reads (ordered) --------------------
static inline uint64_t tsc_start() {
    unsigned lo, hi;
    asm volatile(
        "lfence\n\t"
        "rdtsc\n\t"
        //"lfence\n\t"
        : "=a"(lo), "=d"(hi)
        :
        : "memory"
    );
    return (uint64_t(hi) << 32) | lo;
}

static inline uint64_t tsc_stop(unsigned* aux_out = nullptr) {
    unsigned lo, hi, aux;
    asm volatile(
        //"mfence\n\t"
        "rdtscp\n\t"
        "lfence\n\t"
        : "=a"(lo), "=d"(hi), "=c"(aux)
        :
        : "memory"
    );

    if (aux_out) *aux_out = aux;
    return (uint64_t(hi) << 32) | lo;
}

// -------------------- clock domain: CLOCK_MONOTONIC only --------------------
static inline uint64_t now_ns() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return uint64_t(ts.tv_sec) * 1000000000ull + uint64_t(ts.tv_nsec);
}

static inline timespec ns_to_ts(uint64_t ns) {
    timespec ts;
    ts.tv_sec  = ns / 1000000000ull;
    ts.tv_nsec = ns % 1000000000ull;
    return ts;
}

static inline void sleep_until_ns(uint64_t target_ns) {
    timespec ts = ns_to_ts(target_ns);
    // Sleep until absolute time in the SAME clock domain (CLOCK_MONOTONIC)
    for (;;) {
        int rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, nullptr);
        if (rc == 0) return;
        if (rc == EINTR) continue;
        std::cerr << "clock_nanosleep failed rc=" << rc << "\n";
        std::abort();
    }
}

// -------------------- calibration ratio & integer conversion --------------------
struct CalibRatio {
    uint64_t dc;     // delta TSC
    uint64_t dt_ns;  // delta ns (CLOCK_MONOTONIC)
};

static inline uint64_t cycles_to_ns(uint64_t cycles, const CalibRatio& r) {
    __int128 num = (__int128)cycles * (__int128)r.dt_ns;
    num += r.dc / 2;                 // round-to-nearest
    return (uint64_t)(num / r.dc);
}

static CalibRatio calibrate_once(uint64_t duration_ns) {
    uint64_t t0 = now_ns();
    uint64_t c0 = tsc_start();

    // Busy-wait is fine for 200~500ms calibration windows
    while (now_ns() - t0 < duration_ns) {}

    uint64_t c1 = tsc_stop();
    uint64_t t1 = now_ns();

    return CalibRatio{ .dc = (c1 - c0), .dt_ns = (t1 - t0) };
}

static CalibRatio calibrate_median(int rounds = 7, uint64_t duration_ns = 300000000ull /*300ms*/) {
    std::vector<CalibRatio> v;
    v.reserve(rounds);
    for (int i = 0; i < rounds; ++i) v.push_back(calibrate_once(duration_ns));

    auto less_hz = [](const CalibRatio& a, const CalibRatio& b) {
        // Compare a.dc/a.dt_ns < b.dc/b.dt_ns without float:
        __int128 left  = (__int128)a.dc * (__int128)b.dt_ns;
        __int128 right = (__int128)b.dc * (__int128)a.dt_ns;
        return left < right;
    };

    std::nth_element(v.begin(), v.begin() + rounds/2, v.end(), less_hz);
    return v[rounds/2];
}

int main() {
    std::cout << "sched_getcpu()=" << sched_getcpu() << "\n";

    CalibRatio r = calibrate_median(7, 300000000ull); // 300ms

    long double tsc_hz = (long double)r.dc * 1e9L / (long double)r.dt_ns;
    std::cout << "calib: dc=" << r.dc << " dt_ns=" << r.dt_ns
              << " => tsc_hz≈" << (double)tsc_hz << " Hz\n";

    // Measure ~1s using absolute sleep in the same clock domain
    uint64_t wall0 = now_ns();
    uint64_t target = wall0 + 1000000000ull; // 1s

    unsigned aux = 0;
    uint64_t c0 = tsc_start();
    sleep_until_ns(target - 200000ull); // 200us
    // tiny spin to remove the last few hundred ns of overshoot
    while (now_ns() < target) { 
        //asm volatile("pause"); 
    }
    uint64_t c1 = tsc_stop(&aux);

    uint64_t cycles = c1 - c0;
    uint64_t ns_est = cycles_to_ns(cycles, r);
    uint64_t wall_ns = now_ns() - wall0;

    std::cout << "cyc " << cycles
              << " ns_est " << ns_est
              << " wall_ns " << wall_ns
              << " aux " << aux
              << " aux_cpu " << (aux & 0xfff)
              << " aux_node " << (aux >> 12)
              << "\n";

    std::cout << "sched_getcpu()=" << sched_getcpu() << "\n";
}

