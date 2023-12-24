#include <bits/stdc++.h>
using namespace std;

/******
 * todo: what is basic_string?
 */

/******
 * quick reference:
 *
 * ref = front()
 * ref = back()
 * str substr(index=0, count=npos)
 *
 * =
 * const_ref = at(index)
 * const_ref = operator[](index)
 *
 * char& front()
 * char& back()
 * 
 * char* c_str() // guarantee to be end with NULL
 * char* data()  // guarantee to be end with NULL
 * string_view operator
 *
 * clear()
 * insert()
 * erase(index=0, count=npos)
 * erase(it)
 * erase(first_it, last_it)
 * void push_back(char)
 * void pop_back()
 * string& append()
 * operator +=
 * replace(index, count, new_str)
 * n_copied copy(dest, count, start_index) // no guarantee to be NULL end
 * resize(len, char) // fill new space with char
 *
 * format("{}", val) // c26
 *
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
 *
 * non-member
 * operator +
 * n_removed erase(str, char_to_del)                   // c20
 * n_removed erase_if(str, [](char x){ return true; }) // c20
 * 
 * IO operator
 * >>
 * <<
 * getline
 *
 * conversion
 * to_string(val)
 * stoi
 * stoll
 * stod
 *
 * // high performance, do not depends on c lib
 * // local-independent, non-allocating, non-throwing
 * // guaratee to convert equal value back and forth
 * to_chars    // c17
 * from_chars  // c17
 */
void typical_usage() {
  cout << __func__ << endl;

  string str{"abcde"};
  cout << str.front() << " " << str[2] << " " << str.back() << endl;
}


/******
 * initialization
 */
void init_example() {
  cout << __func__ << endl;

  auto str = "abc"s;   // c14
}


/******
 * advanced usage
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
