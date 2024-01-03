#include <bits/stdc++.h>
using namespace std;

template <typename T>
constexpr auto type_name() {
  std::string_view prefix{ "constexpr auto type_name() [with T = " };
  std::string_view suffix{ "]" };
  std::string_view name{ __PRETTY_FUNCTION__ };
  name.remove_prefix(prefix.size());
  name.remove_suffix(suffix.size());
  return name;
}

// 供 print_template_param_type 内部使用
// 接收一个类型参数，用法 print_type_type(int);
#define print_type_type(X) std::cout << type_name<X>() << std::endl;

// 最基本的打印函数
// 接收一个变量或常量，用法 print_var_type(42);
#define print_var_type(X) std::cout << type_name<decltype(X)>() << std::endl;


// 使用举例
// 打印多个模板参数的实现
template<typename T>
void print_template_param_type() { print_type_type(T); }



struct Obj {
    Obj() = default;
    Obj (int x) : val(x) {}
    int val = 0;
    void use() {
        ++val;
        // cout << "inside " << val << endl;
    }
};

template<typename Want, typename Have>
inline std::conditional_t<std::is_same_v<Want, Have>, Want &&, Want>
local_copy(Have &in)
{
// want的类型总是obj
// have只有在入参是右值临时变量时，类型是obj，于want相同
//     此时返回值是obj&&，直接移动对象
// have在入参是左值时，类型是obj&
//     入参是int时，就是int
//     此时返回值是Obj，会复制对象
   cout << "is same " << std::is_same_v<Want, Have> << endl;
   cout<<"local_copy want-> ";
   print_template_param_type<Want>();

   cout<<"local_copy have-> ";
   print_template_param_type<Have>();

   cout<<"local_copy return-> ";
   print_var_type(static_cast<Have&&>(in));
   return static_cast<Have&&>(in);
}

template<std::convertible_to<Obj> ...T>
void
good1(T&&...t)
{
  // Unary fold over comma operator
  (local_copy<Obj, T>(t).use(), ...);
}

// Another way to do it
template<std::convertible_to<Obj> ...T>
void
good2(T&&...t)
{
  auto use = []<typename U>(U &&arg) {
    decltype(auto) o = local_copy<Obj, U>(arg);
    cout << "good2-> ";
    print_var_type(o);
    o.use();
  };
  (use(std::forward<T>(t)), ...);
}



int main()
{
    Obj a;
    a.val = 42;
    // good1(a);
    good2(a);
    cout << "outside " << a.val << endl;

    // static_assert(std::is_same_v<Obj, decltype(a)> == true);
    // static_assert(std::is_same_v<Obj, decltype(Obj{3})> == true);
    // static_assert(std::is_same_v<Obj, decltype(3)> == false);

    // good1(Obj{42});
    good2(Obj{42});

    // good1(38);
    good2(38);

    // cout << "out " << a.val << endl;

    return 0;
}

