#include <bits/stdc++.h>
using namespace std;


/******
 * quick reference:
 *
 * ref = at(index)       // with runtime bound check
 * ref = arr[index]
 *
 * ref = front()
 * ref = back()
 *
 * fill(val)             // fill all elements with val
 *
 * non-member
 * ref = get<index>(arr)        // compile time boundary check
 * std::array = to_array({...}) // after c20 length deduction
 *
 * compile time
 * tuple_size    // the number of elements as a compile-time constant expression
 * tuple_element // compile-time indexed access to the type of the elements
 */
void typical_usage() {
  cout << __func__ << endl;

  array<int, 6> arr{1, 2, 3, 4, 5, 6};
  cout << arr[2] << " " << arr.front() << " " << arr.back() << endl;
  cout << get<2>(arr) << endl;
}


/******
 * initialization
 */
void init_example() {
  cout << __func__ << endl;

  // auto arr = array<int>{1, 2, 3};       // wrong, missing N
  auto arr1 = to_array<int>({1, 2, 3, 4}); // no need to provide N
  auto arr2 = array{"foo"};                // got std::array<const char*, 1>
  auto arr3 = to_array("foo");             // got std::array<char, 4>
}


/******
 * compile time
 */
void compile_time() {
  cout << __func__ << endl;

  array<int, 6> arr{1, 2, 3, 4, 5, 6};
  static_assert(tuple_size<decltype(arr)>{} == 6);

  using T = tuple_element<0, decltype(arr)>::type;
  static_assert(is_same_v<T, int>);
}

/******
 * main
 */
int main() {
  typical_usage();
  init_example();
  compile_time();
  return 0;
}
