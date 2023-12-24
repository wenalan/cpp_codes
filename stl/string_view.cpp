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
 * void remove_prefix
 * void remove_suffix
 *
 * copy(dest_addr, count)
 * substr(index, count)
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
void init_example() {
  cout << __func__ << endl;

  string_view sv{"foo"};
  string_view sv1("12345", 3);
  cout << sv1 << endl;

  string str{"abcde"};
  string_view sv2(str.begin(), str.begin()+3);
  cout << sv2 << endl;
}


/******
 * copy
 */
void copy() {
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
  sv2.copy(dest.data(), 4);
  cout << dest.data() << endl;
}


/******
 * main
 */
int main() {
  typical_usage();
  init_example();
  copy();
  return 0;
}
