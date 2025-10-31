// clang++ -O3 -std=c++17 -pthread false_sharing_bench.cpp -o bench

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <iomanip>
#include <cstring>

#if defined(__APPLE__)
#include <mach/thread_policy.h>
#include <mach/mach.h>
#elif defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif

// ====================== Case 1: False sharing ======================
struct FS_Bad {
    std::atomic<long long> a{0};
    std::atomic<long long> b{0};
};

// ====================== Case 2: Padded (no false sharing) ======================
struct alignas(64) FS_Good {
    std::atomic<long long> a{0};
    char pad1[64 - sizeof(std::atomic<long long>)];
    std::atomic<long long> b{0};
    char pad2[64 - sizeof(std::atomic<long long>)];
};

#if defined(__APPLE__)
void pin_thread_macos(int core_id) {
    thread_affinity_policy_data_t policy = {core_id};
    thread_policy_set(
        mach_thread_self(),
        THREAD_AFFINITY_POLICY,
        (thread_policy_t)&policy,
        1);
}
#elif defined(__linux__)
void pin_thread_linux(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}
#endif

template <typename T>
void run_benchmark(const char* name) {
    T data;
    constexpr size_t ITER = 200'000'000;  // 2e8 iterations
    auto start = std::chrono::high_resolution_clock::now();

    std::thread t1([&] {
#if defined(__APPLE__)
        pin_thread_macos(0);
#elif defined(__linux__)
        pin_thread_linux(0);
#endif
        for (size_t i = 0; i < ITER; ++i)
            data.a.fetch_add(1, std::memory_order_relaxed);
    });

    std::thread t2([&] {
#if defined(__APPLE__)
        pin_thread_macos(1);
#elif defined(__linux__)
        pin_thread_linux(1);
#endif
        for (size_t i = 0; i < ITER; ++i)
            data.b.fetch_add(1, std::memory_order_relaxed);
    });

    t1.join();
    t2.join();

    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << std::left << std::setw(20) << name
              << " time: " << std::fixed << std::setprecision(2)
              << ms << " ms" << std::endl;
}

int main() {
    std::cout << "Running atomic false-sharing test...\n";
    run_benchmark<FS_Bad>("❌ False Sharing");
    run_benchmark<FS_Good>("✅ No False Sharing");
    return 0;
}

