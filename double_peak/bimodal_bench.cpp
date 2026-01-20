// bimodal_bench.cpp
// Build: g++ -O2 -march=native -std=c++20 -pthread bimodal_bench.cpp -o bimodal_bench
// Run:   ./bimodal_bench --iters 200000 --thrash-prob 0.5 --out out.csv

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include <sched.h>
#include <unistd.h>
#include <sys/mman.h>

// -----------------------------
// Low-level TSC timing helpers
// -----------------------------
// Use RDTSCP (ordered wrt later instructions) + LFENCE to serialize.
static inline uint64_t rdtscp_serialized() {
  unsigned aux;
  uint64_t t;
  asm volatile("lfence\n\t"
               "rdtscp\n\t"
               "lfence\n\t"
               : "=a"(t), "=d"(*((uint32_t*)&t + 1)), "=c"(aux)
               :
               : "memory");
  return t;
}

// Prevent compiler from moving code across this point.
static inline void compiler_barrier() {
  asm volatile("" ::: "memory");
}

// -----------------------------
// Pin to a single CPU core
// -----------------------------
static void pin_to_cpu0() {
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(0, &set);
  if (sched_setaffinity(0, sizeof(set), &set) != 0) {
    perror("sched_setaffinity");
    // Not fatal; continue.
  }
}

// Optional: reduce paging noise
static void lock_memory_best_effort() {
  // Lock current + future mappings. If it fails (no privilege), just continue.
  mlockall(MCL_CURRENT | MCL_FUTURE);
}

// -----------------------------
// "Victim" code: very short, to make frontend effects visible.
// Keep it noinline so call/ret + BTB also participate.
// -----------------------------
static std::atomic<uint64_t> g_sink{0};

__attribute__((noinline))
static void victim() {
  // A tiny sequence: a couple of ALU ops + a dependent store-like effect.
  // Keep some data dependency to avoid full elimination.
  uint64_t x = g_sink.load(std::memory_order_relaxed);
  x = x * 1315423911ull + 0x9e3779b97f4a7c15ull;
  // A few nops so the code isn't literally 1-2 uops
  asm volatile("nop\n\tnop\n\tnop\n\tnop\n\t" ::: "memory");
  g_sink.store(x, std::memory_order_relaxed);
}

// -----------------------------
// Frontend "thrash": pollute I-cache + BTB by jumping across lots of code.
// We generate many distinct noinline functions and call them via a function table.
// -----------------------------
using Fn = void(*)();

#define GEN_DUMMY_FN(N)                       \
  __attribute__((noinline)) static void d##N() { \
    asm volatile(                             \
      "nop\n\tnop\n\tnop\n\tnop\n\t"          \
      "nop\n\tnop\n\tnop\n\tnop\n\t"          \
      ::: "memory");                          \
  }

GEN_DUMMY_FN(0)   GEN_DUMMY_FN(1)   GEN_DUMMY_FN(2)   GEN_DUMMY_FN(3)
GEN_DUMMY_FN(4)   GEN_DUMMY_FN(5)   GEN_DUMMY_FN(6)   GEN_DUMMY_FN(7)
GEN_DUMMY_FN(8)   GEN_DUMMY_FN(9)   GEN_DUMMY_FN(10)  GEN_DUMMY_FN(11)
GEN_DUMMY_FN(12)  GEN_DUMMY_FN(13)  GEN_DUMMY_FN(14)  GEN_DUMMY_FN(15)
GEN_DUMMY_FN(16)  GEN_DUMMY_FN(17)  GEN_DUMMY_FN(18)  GEN_DUMMY_FN(19)
GEN_DUMMY_FN(20)  GEN_DUMMY_FN(21)  GEN_DUMMY_FN(22)  GEN_DUMMY_FN(23)
GEN_DUMMY_FN(24)  GEN_DUMMY_FN(25)  GEN_DUMMY_FN(26)  GEN_DUMMY_FN(27)
GEN_DUMMY_FN(28)  GEN_DUMMY_FN(29)  GEN_DUMMY_FN(30)  GEN_DUMMY_FN(31)
GEN_DUMMY_FN(32)  GEN_DUMMY_FN(33)  GEN_DUMMY_FN(34)  GEN_DUMMY_FN(35)
GEN_DUMMY_FN(36)  GEN_DUMMY_FN(37)  GEN_DUMMY_FN(38)  GEN_DUMMY_FN(39)
GEN_DUMMY_FN(40)  GEN_DUMMY_FN(41)  GEN_DUMMY_FN(42)  GEN_DUMMY_FN(43)
GEN_DUMMY_FN(44)  GEN_DUMMY_FN(45)  GEN_DUMMY_FN(46)  GEN_DUMMY_FN(47)
GEN_DUMMY_FN(48)  GEN_DUMMY_FN(49)  GEN_DUMMY_FN(50)  GEN_DUMMY_FN(51)
GEN_DUMMY_FN(52)  GEN_DUMMY_FN(53)  GEN_DUMMY_FN(54)  GEN_DUMMY_FN(55)
GEN_DUMMY_FN(56)  GEN_DUMMY_FN(57)  GEN_DUMMY_FN(58)  GEN_DUMMY_FN(59)
GEN_DUMMY_FN(60)  GEN_DUMMY_FN(61)  GEN_DUMMY_FN(62)  GEN_DUMMY_FN(63)

static Fn g_fns[] = {
  d0,d1,d2,d3,d4,d5,d6,d7,d8,d9,d10,d11,d12,d13,d14,d15,
  d16,d17,d18,d19,d20,d21,d22,d23,d24,d25,d26,d27,d28,d29,d30,d31,
  d32,d33,d34,d35,d36,d37,d38,d39,d40,d41,d42,d43,d44,d45,d46,d47,
  d48,d49,d50,d51,d52,d53,d54,d55,d56,d57,d58,d59,d60,d61,d62,d63
};

__attribute__((noinline))
static void thrash_frontend(std::mt19937_64& rng) {
  // Indirect calls across many targets:
  // - pollute BTB / indirect predictor
  // - touch a bunch of distinct code locations (I-cache)
  // We keep it deterministic length to avoid adding its own “variable time” noise.
  std::uniform_int_distribution<int> dist(0, (int)(sizeof(g_fns)/sizeof(g_fns[0])) - 1);

  // Fixed number of calls; pick random targets.
  for (int i = 0; i < 256; ++i) {
    g_fns[dist(rng)]();
  }

  // Also touch some data to shake D-cache a bit (optional)
  alignas(64) static uint8_t buf[256 * 1024];
  for (size_t off = 0; off < sizeof(buf); off += 4096) {
    buf[off] ^= (uint8_t)off;
  }
  compiler_barrier();
}

// -----------------------------
// CLI args
// -----------------------------
struct Args {
  int iters = 200000;
  double thrash_prob = 0.5;
  std::string out = "out.csv";
  int warmup = 2000;
};

static void usage(const char* prog) {
  std::fprintf(stderr,
    "Usage: %s [--iters N] [--warmup N] [--thrash-prob P] [--out FILE]\n"
    "  --iters       number of samples (default 200000)\n"
    "  --warmup      warmup iterations not recorded (default 2000)\n"
    "  --thrash-prob probability of running thrash before each sample (0..1, default 0.5)\n"
    "  --out         output CSV (default out.csv)\n", prog);
}

static Args parse_args(int argc, char** argv) {
  Args a;
  for (int i = 1; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--iters") && i + 1 < argc) a.iters = std::atoi(argv[++i]);
    else if (!std::strcmp(argv[i], "--warmup") && i + 1 < argc) a.warmup = std::atoi(argv[++i]);
    else if (!std::strcmp(argv[i], "--thrash-prob") && i + 1 < argc) a.thrash_prob = std::atof(argv[++i]);
    else if (!std::strcmp(argv[i], "--out") && i + 1 < argc) a.out = argv[++i];
    else if (!std::strcmp(argv[i], "--help")) { usage(argv[0]); std::exit(0); }
    else { usage(argv[0]); std::exit(1); }
  }
  if (a.thrash_prob < 0.0) a.thrash_prob = 0.0;
  if (a.thrash_prob > 1.0) a.thrash_prob = 1.0;
  return a;
}

int main(int argc, char** argv) {
  Args args = parse_args(argc, argv);

  pin_to_cpu0();
  lock_memory_best_effort();

  std::mt19937_64 rng(0xC0FFEE123456789ULL);
  std::bernoulli_distribution do_thrash(args.thrash_prob);

  // Warm up: try to stabilize frequency / caches a bit
  for (int i = 0; i < args.warmup; ++i) {
    victim();
  }

  std::FILE* f = std::fopen(args.out.c_str(), "w");
  if (!f) {
    perror("fopen");
    return 1;
  }
  std::fprintf(f, "i,thrash,cycles\n");

  for (int i = 0; i < args.iters; ++i) {
    int thrash = do_thrash(rng) ? 1 : 0;
    if (thrash) thrash_frontend(rng);

    compiler_barrier();
    uint64_t t0 = rdtscp_serialized();
    victim();
    uint64_t t1 = rdtscp_serialized();
    compiler_barrier();

    uint64_t cycles = t1 - t0;
    std::fprintf(f, "%d,%d,%llu\n", i, thrash, (unsigned long long)cycles);
  }

  std::fclose(f);
  return 0;
}
