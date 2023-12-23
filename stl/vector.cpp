#include <bits/stdc++.h>
using namespace std;


/******
 * todo: how copy / move assignment was written
 */

/******
 * quick reference:
 *
 * push_back(val)
 * clear()
 *
 * ref = front()
 * ref = back()
 *
 * insert(it, val)
 * insert(it, count, val)
 * insert(it, {})
 * insert(it, first_it, last_it)
 * 
 * emplace(it, args...)
 * emplace_back(args...)
 *
 * erase(it)
 * erase(first_it, last_it)
 */
void typical_usage() {
  cout << __func__ << endl;

  vector<int> v; 
  v.push_back(42);
  auto val = v.front(); // return ref to the front element
  v.pop_back();              // destroy the front element
  cout << val << " " << v.size() << endl;  // safe, as val is a copy of int

  v.clear();
}


/******
 * initialization
 */
void init_example() {
  cout << __func__ << endl;

  vector<int> v1{10};    // 1 element with value 10
  vector<int> v2(10);    // 10 elements with value 0
  vector<int> v3(10, 1); // 10 elements with value 1
  vector<int> v4{10, 1}; // 2 elements with value 10 and 1

  vector<string> v5{"hi"};     // one string element "hi"
  // vector<string> v("hi");   // wrong, cannot compile
  vector<string> v6{10, "hi"}; // 10 string elements with "hi"
  vector<string> v7{10};       // fall back to ctor param, if param is not acceptable
                               // result is 10 string elements with default value

  // two-dimension vector
  int n_rows = 2;
  int n_cols = 3;
  int init_val = 42;
  vector<vector<int>> matrix(n_rows, vector<int>(n_cols, init_val));

  cout << matrix.size() << " " << matrix[0].size() << endl;
}


/******
 * sort examples
 */
vector<vector<int>> sort_example() {
  cout << __func__ << endl;

  vector<vector<int>> vec{{1, 2}, {5, 6}, {3, 4}, {5, 3}};
  // descending ordering first by vec[0] then vec[1]
  sort(vec.begin(), vec.end(), greater<>());

  for (auto const& line : vec) {
    for (auto i : line) {
      cout << i << " ";
    }
    cout << endl;
  }

  return {}; // return empty two-dimension vector
}


/******
 * main
 */
int main() {
  typical_usage();
  init_example();
  sort_example();
  return 0;
}
