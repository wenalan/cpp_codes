#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

static void leak_new(size_t n, int times) {
    for (int i = 0; i < times; i++) {
        // 故意泄漏：new 出来后把指针丢掉，不 delete
        char* p = new char[n];
        p[0] = 0x42;
        (void)p;
    }
}

static void leak_malloc(size_t n, int times) {
    for (int i = 0; i < times; i++) {
        // 故意泄漏：malloc 后不 free
        void* p = std::malloc(n);
        if (!p) {
            std::cerr << "malloc failed\n";
            std::exit(2);
        }
        std::memset(p, 0, n);
        (void)p;
    }
}

static void no_leak(size_t n, int times) {
    for (int i = 0; i < times; i++) {
        // 不泄漏：用 unique_ptr 自动释放
        auto p = std::make_unique<char[]>(n);
        p[0] = 0x7;
    }
}

int main(int argc, char** argv) {
    // 用法：
    //   ./myapp --leak        （默认行为：制造泄漏）
    //   ./myapp --no-leak     （不制造泄漏）
    //   ./myapp --both        （先不泄漏，再泄漏，方便对照）
    std::string mode = (argc >= 2) ? argv[1] : "--leak";

    const size_t bytes = 1 << 20; // 1 MiB
    const int times = 8;          // 泄漏 8 次 -> 约 8 MiB

    std::cout << "mode=" << mode << ", bytes=" << bytes << ", times=" << times << "\n";

    if (mode == "--no-leak") {
        no_leak(bytes, times);
        std::cout << "done: no leak path\n";
    } else if (mode == "--both") {
        no_leak(bytes, times);
        leak_new(bytes, times);
        leak_malloc(bytes / 2, times); // 再加一些 malloc 泄漏
        std::cout << "done: both path\n";
    } else { // "--leak" or anything else
        leak_new(bytes, times);
        leak_malloc(bytes / 2, times);
        std::cout << "done: leak path\n";
    }

    return 0; // HeapChecker 通常在进程退出时汇总并报泄漏
}
