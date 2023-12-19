#include <bits/stdc++.h>
using namespace std;

/******
 * for leetcode:
 *
 * val = top()
 * push(val)
 * pop()
 */

/******
 * typical usage
 */
void typical_usage() {
  stack<int> stk;
  stk.push(42);
  auto val = stk.top(); // return ref to the top element
  stk.pop();            // destroy the top element
  cout << val << endl;  // safe, as val is copy of int
}

/******
 * notes:
 *
 * no iterator supporting, no begin() or end()
 */

/******
 * emplace
 */
void usage1() {
  stack<int> stk;
  stk.emplace();

  stk = {};   // clear all, but no clear() method
}

/******
 * main
 */
auto main() -> int {
  typical_usage();
  return 0;
}
