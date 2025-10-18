#include <iostream>
#include <type_traits>
#include <iostream>
#include <concepts>
#include <string>
using namespace std;

/* 
     void print_obj(obj)

     // if obj has a member to_string call it obj.to_string
     // if not call std::to_string(obj)
 */


// define concept
template<typename T>
concept HasMemberToString = requires(const T& obj) {
    { obj.to_string() } -> std::convertible_to<std::string>;
};
// 严格来讲还需要一个(T&& obj)签名的concept，否则会遗漏参数是右值的情况
// 就是说如果obj只有一个接收右值obj的to_string函数，那么是不会满足这个concept的

template<typename T>
concept HasStdToString = requires(const T& obj) {
    { std::to_string(obj) } -> std::convertible_to<std::string>;
};

// has obj.to_string
template<typename T>
requires HasMemberToString<T>
// 上面两行可以合并成template<HasMemberToString T>
void print_obj(const T& obj)  {
    std::cout << obj.to_string() << std::endl;
}

// no obj.to_string
template<typename T>
requires (!HasMemberToString<T> && HasStdToString<T>)
// 这样写避免同时满足两个条件时可能发生的歧义
void print_obj(const T& obj) {
    std::cout << std::to_string(obj) << std::endl;
}

// testing
struct WithToString {
  std::string to_string() const {
    return "has to_string";
  }
};

struct WithoutToString {
  int a {123};
  operator int() const {return a;} // 这里当时没有看懂，所以没有写这一行
  // std::to_string() 只接受数字类型的参数，例如int，long等
  // 所以必须提供转换函数，才能让to_string接收自定义类型
};


// To execute C++, please define "int main()"
int main() {

  WithToString a;
  WithoutToString b;

  print_obj(a);
  print_obj(b);  // 因为没有50行，所以这里报错了
                 // 因为std::to_string不接收自定义类型
                 // 所以一定要加一个转换成int的函数才行

  return 0;
}