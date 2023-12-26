#include <bits/stdc++.h>
using namespace std;

void print_strings(std::convertible_to<std::string_view> auto&& ...s) {
  for (auto v : std::initializer_list<std::string_view>{ s... })
    std::cout << v << " ";
  std::cout << std::endl;
}

int main() {
  print_strings("one", std::string{"two"});
}