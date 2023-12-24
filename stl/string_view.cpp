#include <bits/stdc++.h>
using namespace std;

/******
 * todo: what is basic_string_view?
 */

/******
 * quick reference: // after c20
 *
 * ref = front()
 * ref = back()
 *
 * =
 * const_ref = at(index)
 * const_ref = operator[](index)
 *
 * void remove_prefix(n) // remove n chars from head
 * void remove_suffix(n) // remove n chars from tail
 *
 * char* data() // no guarantee to be end with NULL
 *
 * count copy(dest_addr, count, start_index=0)
 * sv substr(index, count)
 * int compare(another_sv)
 * bool starts_with(sv) // c20
 * bool ends_with(sv)   // c20
 * bool contains(sv)    // c23
 *
 * index find(sv, start_index=0)
 * index rfind(sv, start_index=npos)
 * index find_first_of(sv, start_index=0)
 * index find_last_of(sv, start_index=npos)
 * index find_first_not_of(sv, start_index=0)
 * index find_last_not_of(sv, start_index=npos)
 */
void typical_usage() {
  cout << __func__ << endl;

  string str{"foo"};
  string_view sv(str); // first, str converted to string_view
  cout << sv[0] << endl;
}


/******
 * initialization
 */
string get_string() {
  return string{"abc"};
}

int f(string_view sv) {
  return 1;
}

void init_example() {
  cout << __func__ << endl;

  auto sv = "abc"sv;

  string_view sv1{"foo"};
  string_view sv2("12345", 3);
  cout << sv2 << endl;

  string str{"abcde"};
  string_view sv3(str.begin(), str.begin()+3);
  cout << sv3 << endl;

  auto res = f(get_string());     // string is OK convert to string_view directly
  string_view sv4 = get_string(); // Bad, holds a dangling pointer

  // string literals reside in persistent data storage
  string_view good{"a string literal"};
  // dangling, as the temproray string is destroyed by the end of this statment
  string_view bad{"a temproray string"s};
}


/******
 * copy_example
 */
void copy_example() {
  cout << __func__ << endl;
  
  string str{"abcde"};
  string_view sv1(str.begin(), str.end());
  string_view sv2 = sv1;
  cout << sv1[1] << " " << sv2[1] << endl;

  // no connection between sv1 and sv2
  sv1.remove_prefix(2);
  cout << sv1 << " " << sv2 << endl;
  sv1.remove_suffix(1);
  cout << sv1 << " " << sv2 << endl;
  
  array<char, 8> dest;
  dest.fill(0);
  sv2.copy(dest.data(), 3, 1);
  cout << dest.data() << endl;
}


/******
 * main
 */
int main() {
  typical_usage();
  init_example();
  copy_example();
  return 0;
}
