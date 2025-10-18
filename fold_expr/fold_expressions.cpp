#include <iostream>
#include <vector>
#include <string>
#include <utility>

// 1) 求和：二元左折叠（有“幺元”0，允许空参数包）
template <typename... Ts>
auto sum(Ts... xs) {
  return (0 + ... + xs);
    // ((0 + x1) + x2) + ... + xn
}

// 2) 逻辑与：一元右折叠（无幺元，不允许空包）
template <typename... Ts>
bool all_true(Ts... bools) {
  return (bools && ...);
    // (b1 && (b2 && (... && bn)))
}

// 3) 向容器 push_back：利用逗号运算符做“逐个副作用”
template <typename T, typename... Ts>
void push_all(std::vector<T>& v, Ts&&... ts) {
  (v.push_back(std::forward<Ts>(ts)), ...);
    // (push(x1), push(x2), ..., push(xn))
}

// 4) 字符串拼接：二元右折叠（显式初值，允许空包）
template <typename... Ts>
std::string join_with_comma(Ts&&... ts) {
  return (std::string{} + ... + (std::string(std::forward<Ts>(ts)) + ","));
    // 末尾会多一个逗号
}

// 5) 完美转发调用：把同一个可调用对象对每个参数调用一次
template <typename F, typename... Args>
void for_each_arg(F&& f, Args&&... args) {
  auto&& fn = std::forward<F>(f);
  (fn(std::forward<Args>(args)), ...);
}

int main() {
    std::cout << sum(1, 2, 3, 4) << "\n";          // 10
    std::cout << std::boolalpha << all_true(true, true, false) << "\n"; // false

    std::vector<int> v;
    push_all(v, 3, 1, 4, 1, 5);                    // v = {3,1,4,1,5}
    for (auto x : v) std::cout << x << ' ';        // 3 1 4 1 5
    std::cout << "\n";

    std::cout << join_with_comma("ab", "cd", "ef") << "\n";  // "ab,cd,ef,"

    for_each_arg([](auto&& x){ std::cout << "[" << x << "]"; }, 42, "hi", 3.14);
    std::cout << "\n";
}
