#include <atomic>
#include <thread>
#include <iostream>
#include <set>

enum Mode { RELAXED, ACQ_REL, SEQ_CST };

void run_test(Mode mode, const char* name) {
    std::atomic<int> x{0}, y{0};
    int r1 = 0, r2 = 0;
    std::set<std::pair<int,int>> seen;

    for (size_t iter = 0; iter < 1'000'000; ++iter) {
        x = 0; y = 0; r1 = r2 = 0;

        std::thread t1([&]() {
            switch (mode) {
                case RELAXED:
                    x.store(1, std::memory_order_relaxed);
                    r1 = y.load(std::memory_order_relaxed);
                    break;
                case ACQ_REL:
                    x.store(1, std::memory_order_release);
                    r1 = y.load(std::memory_order_acquire);
                    break;
                case SEQ_CST:
                    x.store(1, std::memory_order_seq_cst);
                    r1 = y.load(std::memory_order_seq_cst);
                    break;
            }
        });

        std::thread t2([&]() {
            switch (mode) {
                case RELAXED:
                    y.store(1, std::memory_order_relaxed);
                    r2 = x.load(std::memory_order_relaxed);
                    break;
                case ACQ_REL:
                    y.store(1, std::memory_order_release);
                    r2 = x.load(std::memory_order_acquire);
                    break;
                case SEQ_CST:
                    y.store(1, std::memory_order_seq_cst);
                    r2 = x.load(std::memory_order_seq_cst);
                    break;
            }
        });

        t1.join();
        t2.join();

        auto p = std::make_pair(r1, r2);
        if (!seen.count(p)) {
            seen.insert(p);
            std::cout << "[" << name << "] Observed: (" << r1 << ", " << r2 << ")\n";
        }

        if (seen.size() == 4) break; // 所有组合都出现就退出
    }

    std::cout << "[" << name << "] Total unique outcomes: " << seen.size() << "\n\n";
}

int main() {
    std::cout << "Running store–load reordering test\n\n";

    run_test(RELAXED, "RELAXED");
    run_test(ACQ_REL, "ACQ_REL");
    run_test(SEQ_CST, "SEQ_CST");

    return 0;
}
