#include <bits/stdc++.h>
using namespace std;


/******
 * notes:
 * no iterator supporting, no begin() or end() methods
 */


/******
 * quick reference:
 *
 * val = front()
 * val = back()
 * push(val)
 * pop()
 */
void typical_usage() {
  queue<int> q;         // after c23, can construct from iterator
  q.push(42);
  auto val = q.front(); // return ref to the front element
  q.pop();              // destroy the front element
  cout << val << endl;  // safe, as val is a copy of int
}


/******
 * initialization
 */
void init_example() {
  queue<int> q{{1, 2}}; // equal to queue<int> q{deque<int>{1, 2}};
  q = {};               // clear all, as no clear() method
}


/******
 * emplace
 */
struct obj {
  int x;
};

void emplace_example() {
  queue<obj> q;
  auto ref = q.emplace(42); // return ref after c17
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
