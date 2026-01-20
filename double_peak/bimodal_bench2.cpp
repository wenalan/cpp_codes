// bimodal_bench2.cpp
// Build: g++ -O2 -march=native -std=c++20 -pthread bimodal_bench2.cpp -o bimodal_bench2
// Run:   ./bimodal_bench2 --iters 300000 --thrash-prob 0.5 --out out.csv

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>

#include <sched.h>
#include <unistd.h>
#include <sys/mman.h>

// -----------------------------
// TSC timing: LFENCE + RDTSCP + LFENCE
// -----------------------------
static inline uint64_t rdtscp_serialized() {
  uint32_t lo, hi;
  asm volatile(
      "lfence\n\t"
      "rdtscp\n\t"
      "lfence\n\t"
      : "=a"(lo), "=d"(hi)
      :
      : "rcx", "memory");
  return (uint64_t(hi) << 32) | lo;
}

static inline void compiler_barrier() { asm volatile("" ::: "memory"); }

// -----------------------------
// Pin to a single core (CPU0)
// -----------------------------
static void pin_to_cpu0() {
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(0, &set);
  if (sched_setaffinity(0, sizeof(set), &set) != 0) {
    perror("sched_setaffinity");
  }
}

static void lock_memory_best_effort() {
  // If fails, it just means no permission; fine for a demo.
  (void)mlockall(MCL_CURRENT | MCL_FUTURE);
}

// -----------------------------
// Heavy frontend thrash:
// Execute a big block of code so L1I/uop-cache get polluted.
// 128KB NOPs is large enough to exceed L1I (typically 32KB) by a lot.
// We also include a tiny predictable branch pattern.
// -----------------------------
__attribute__((noinline))
static void icache_thrash(int iters) {
  // 'iters' is a loop count for repeating the big block a bit.
  // Keep loop itself predictable.
  for (int k = 0; k < iters; ++k) {
    asm volatile(
        // A little predictable branching to also tickle the branch machinery.
        "xor %%eax, %%eax\n\t"
        "1:\n\t"
        "inc %%eax\n\t"
        "cmp $64, %%eax\n\t"
        "jne 1b\n\t"

        // Big code blob: ~131072 NOPs ~= 128KB
        ".rept 131072\n\t"
        "nop\n\t"
        ".endr\n\t"
        :
        :
        : "eax", "cc", "memory");
  }
}

// -----------------------------
// Victim: "medium" size fixed work.
// Goal: make it sensitive to frontend/caches, not too tiny.
// -----------------------------
static std::atomic<uint64_t> g_sink{0};

// A small data array to avoid everything being register-only.
// 64KB fits in L2 but not necessarily in L1D; we touch a few cache lines.
static uint64_t g_data[8192] __attribute__((aligned(64)));

__attribute__((noinline))
static void victim() {
  uint64_t x = g_sink.load(std::memory_order_relaxed);

  // Fixed amount of work: simple ops + a few memory touches
  // Unroll-ish loop: keeps code moderately sized, adds some real dependency.
  for (int i = 0; i < 256; ++i) {
    // arithmetic
    x = x * 6364136223846793005ull + 1ull;
    x ^= (x >> 17);

    // touch data (stride spreads across cache lines)
    // keep it dependent to avoid being optimized away
    uint64_t idx = (x ^ (uint64_t)i * 1315423911ull) & 8191ull;
    x += g_data[idx];
    g_data[idx] = x;
  }

  // Small asm block to prevent over-optimization/merging
  asm volatile("nop\n\tnop\n\tnop\n\tnop\n\t" ::: "memory");
  g_sink.store(x, std::memory_order_relaxed);
}

// -----------------------------
// CLI args
// -----------------------------
struct Args {
  int iters = 200000;
  int warmup = 5000;
  double thrash_prob = 0.5;
  int thrash_reps = 1;      // how many times to run icache_thrash per sample
  std::string out = "out.csv";
};

static void usage(const char* prog) {
  std::fprintf(stderr,
               "Usage: %s [--iters N] [--warmup N] [--thrash-prob P] [--thrash-reps R] [--out FILE]\n"
               "  --iters       samples recorded (default 200000)\n"
               "  --warmup      warmup iterations (default 5000)\n"
               "  --thrash-prob probability of thrash before a sample (0..1, default 0.5)\n"
               "  --thrash-reps how many icache_thrash blocks per thrash (default 1)\n"
               "  --out         output CSV (default out.csv)\n",
               prog);
}

static Args parse_args(int argc, char** argv) {
  Args a;
  for (int i = 1; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--iters") && i + 1 < argc) a.iters = std::atoi(argv[++i]);
    else if (!std::strcmp(argv[i], "--warmup") && i + 1 < argc) a.warmup = std::atoi(argv[++i]);
    else if (!std::strcmp(argv[i], "--thrash-prob") && i + 1 < argc) a.thrash_prob = std::atof(argv[++i]);
    else if (!std::strcmp(argv[i], "--thrash-reps") && i + 1 < argc) a.thrash_reps = std::atoi(argv[++i]);
    else if (!std::strcmp(argv[i], "--out") && i + 1 < argc) a.out = argv[++i];
    else if (!std::strcmp(argv[i], "--help")) { usage(argv[0]); std::exit(0); }
    else { usage(argv[0]); std::exit(1); }
  }
  if (a.thrash_prob < 0.0) a.thrash_prob = 0.0;
  if (a.thrash_prob > 1.0) a.thrash_prob = 1.0;
  if (a.thrash_reps < 0) a.thrash_reps = 0;
  return a;
}

int main(int argc, char** argv) {
  Args args = parse_args(argc, argv);

  pin_to_cpu0();
  lock_memory_best_effort();

  // Init data to avoid page faults during sampling
  for (size_t i = 0; i < 8192; ++i) g_data[i] = (i * 0x9e3779b97f4a7c15ull) ^ 0x123456789abcdefull;

  std::mt19937_64 rng(0xC0FFEE123456789ULL);
  std::bernoulli_distribution do_thrash(args.thrash_prob);

  // Warmup
  for (int i = 0; i < args.warmup; ++i) {
    victim();
  }

  std::FILE* f = std::fopen(args.out.c_str(), "w");
  if (!f) { perror("fopen"); return 1; }
  std::fprintf(f, "i,thrash,cycles\n");

  for (int i = 0; i < args.iters; ++i) {
    int thr = do_thrash(rng) ? 1 : 0;

    if (thr) {
      // repeat thrash to strengthen cold effect
      icache_thrash(args.thrash_reps);
    }

    compiler_barrier();
    uint64_t t0 = rdtscp_serialized();
    victim();
    uint64_t t1 = rdtscp_serialized();
    compiler_barrier();

    uint64_t cycles = t1 - t0;
    std::fprintf(f, "%d,%d,%llu\n", i, thr, (unsigned long long)cycles);
  }

  std::fclose(f);
  return 0;
}
