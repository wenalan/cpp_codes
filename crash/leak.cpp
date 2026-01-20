#include <iostream>

struct Widget {
    explicit Widget(int v) : value(v) {}
    int value;
};

int main() {
    Widget* leaked = new Widget(42);  // allocated on the heap
    std::cout << "Leaked value: " << leaked->value << std::endl;
    // Intentionally forget to delete to demonstrate a memory leak.
    return 0;
}
