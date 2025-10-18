#include <iostream>

template<typename Derived>
class Base {
    public:
    void interface() {
        static_cast<Derived*>(this)->impl();
    }
};

class Derived1 : public Base<Derived1> {
    public:
    void impl() {
        std::cout << "impl 1\n";
    }
};

class Derived2 : public Base<Derived2> {
    public:
    void impl() {
        std::cout << "impl 2\n";
    }
};

int main() {
    Base<Derived1>* pb1 = new Derived1;
    Base<Derived2>* pb2 = new Derived2;

    pb1->interface();
    pb2->interface();
}