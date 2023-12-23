#include <bits/stdc++.h>
using namespace std;


/******
 * notes:
 * no iterator supporting, no begin() or end() methods
 */


/******
 * quick reference:
 *
 * ref = top()
 * push(val)
 * pop()
 */
void typical_usage() {
  cout << __func__ << endl;

  stack<int> stk;       // after c23, can construct from iterator
  stk.push(42);
  auto val = stk.top(); // return ref to the top element
  stk.pop();            // destroy the top element
  cout << val << endl;  // safe, as val is a copy of int
}


/******
 * initialization
 */
void init_example() {
  cout << __func__ << endl;

  stack<int> s{{1, 2}}; // equal to stack<int> s{deque<int>{1, 2}};
  s = {};               // clear all, as no clear() method
}


/******
 * emplace
 */
struct obj {
  int x;
};

void emplace_example() {
  cout << __func__ << endl;

  stack<obj> s;
  auto ref = s.emplace(42); // return ref after c17
  cout << ref.x << endl;
}


/******
 * main
 */
int main() {
  typical_usage();
  init_example();
  emplace_example();
  return 0;
}
