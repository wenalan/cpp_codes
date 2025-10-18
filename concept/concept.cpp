#include <iostream>
#include <string>
#include <concepts>

struct A {
    std::string to_string() const { return "A"; }
};

struct B {
    std::string to_string() && { return "B"; }
};

template <typename T>
concept HasMemberToString =
    requires(T& a) { { a.to_string() } -> std::convertible_to<std::string>; } ||
    requires(const T& a) { { a.to_string() } -> std::convertible_to<std::string>; } ||
    requires(T&& a) { { std::move(a).to_string() } -> std::convertible_to<std::string>; };

template<HasMemberToString T>
void print_obj(T&& obj) {
    std::cout << std::forward<T>(obj).to_string() << std::endl;
}

int main() {
    A a;
    print_obj(a);           // OK
    print_obj(A{});         // OK
    print_obj(B{});         // OK
}
