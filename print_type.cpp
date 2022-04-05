// https://stackoverflow.com/questions/81870/is-it-possible-to-print-a-variables-type-in-standard-c

#include <iostream>
#include <string_view>

template <typename T>
constexpr auto type_name() {
  std::string_view name, prefix, suffix;
#ifdef __clang__
  name = __PRETTY_FUNCTION__;
  prefix = "auto type_name() [T = ";
  suffix = "]";
#elif defined(__GNUC__)
  name = __PRETTY_FUNCTION__;
  prefix = "constexpr auto type_name() [with T = ";
  suffix = "]";
#elif defined(_MSC_VER)
  name = __FUNCSIG__;
  prefix = "auto __cdecl type_name<";
  suffix = ">(void)";
#endif
  name.remove_prefix(prefix.size());
  name.remove_suffix(suffix.size());
  return name;
}

#define PRINT_TYPE(X) std::cout << type_name<decltype(X)>() << std::endl;

template<typename T>
void print_param_type(T&& t) { PRINT_TYPE(t); }


int main()
{
    int i{ 42 };
    const int& r = i;
    auto a = r;

    PRINT_TYPE(i);PRINT_TYPE(r);PRINT_TYPE(a)
    
    auto test { [](int a){ return 1; } };
    PRINT_TYPE(test);

    PRINT_TYPE(42);
    print_param_type(42);                
    PRINT_TYPE("42");
    print_param_type("42");              
    PRINT_TYPE(std::string{"42"});
    print_param_type(std::string{"42"});
    
    return 0;
}

