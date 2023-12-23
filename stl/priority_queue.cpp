#include <bits/stdc++.h>
using namespace std;


/******
 * notes:
 * no iterator supporting, no begin() or end() methods
 */


/******
 * quick reference:
 *
 * ref = top()        // not front(), as there is no back() method
 * push(val)
 * pop()
 */
void typical_usage() {
  cout << __func__ << endl;

  // by default, use less<>, largest element on top
  priority_queue<int> max_q; // after c23, can construct from iterator

  // use greater<>, smallest element on top
  priority_queue<int, vector<int>, greater<>> min_q;
  min_q.push(42);
  auto val = min_q.top();    // return const ref to the top element
  min_q.pop();               // destroy the top element
  cout << val << endl;       // safe, as val is a copy of int
}


/******
 * initialization
 */
void init_example() {
  cout << __func__ << endl;

  // init by initializer list
  // use less<>, but largest element on top
  priority_queue<int> q1{less<int>(), {1, 2}}; // {1, 2} to vector
  q1.push(3);
  q1 = {};                      // clear all, as no clear() method

  // use greater<>, but smallest element on top
  // greater must be provided twice, when initialized with {1, 2}
  priority_queue<int, vector<int>, greater<int>> q2{greater<int>(), {1, 2}};

  // init by vector iterator
  vector<pair<int, int>> v{{6, 3}, {7, 5}, {1, 8}, {6,5}};
  priority_queue<pair<int, int>, vector<pair<int, int>>, less<>>
	  						q3{v.begin(), v.end()};

  // use tuple as element type
  // sorted by columns from left to right
  // output 2-1-3, 1-3-2, 1-2-3
  priority_queue<tuple<int, int, int>> tupleQ;
  tupleQ.push({1, 2, 3});
  tupleQ.push({2, 1, 3});
  tupleQ.push({1, 3, 2});
  while (!tupleQ.empty()) {
    const auto& [a, b, c] = tupleQ.top();
    cout << a << "-" << b << "-" << c << endl;
    tupleQ.pop();
  }
  cout << endl;
}


/******
 * customized comparation by lambda - preferred method
 */
void order_by_lambda() {
  cout << __func__ << endl;

  auto myComp = [](auto const& a, auto const& b) {
    // sort second in desending order, then first in ascending order
    // the result of priority queue is reversed
    if (a.second == b.second) return a.first < b.first;
    return a.second > b.second;
  };

  vector<pair<int, int>> v{{6, 3}, {7, 5}, {1, 8}, {6,5}};
  priority_queue<pair<int, int>, vector<pair<int, int>>, decltype(myComp)>
	  						q{v.begin(), v.end()};
}


/******
 * customized comparation by function object
 */
struct myComp2 {
  // param type must be pair<int, int>, cannot use auto
  bool operator()(pair<int, int> const& a, pair<int, int> const& b) {
    // sort second in desending order, then first in ascending order
    // the result of priority queue is reversed
    if (a.second == b.second) return a.first < b.first;
    return a.second > b.second;
  }
};

void order_by_function_object() {
  cout << __func__ << endl;

  vector<pair<int, int>> v{{6, 3}, {7, 5}, {1, 8}, {6,5}};
  priority_queue<pair<int, int>, vector<pair<int, int>>, myComp2>
	  						q{v.begin(), v.end()};
}


/******
 * customized comparation for user defined object
 * plus example for emplace
 */
struct obj {
  int x;

  // must be defined to support customized comparation for this object
  // will be overwritten by above lambda or function object, if provided as well
  friend bool operator<(const obj& a, const obj& b) {
    return a.x < b.x; // ascending order, got max element on top
  }
};

void order_by_user_defined_object() {
  cout << __func__ << endl;

  priority_queue<obj> q;
  q.emplace(42);               // return void, even in c23
  cout << q.top().x << endl;
}


/******
 * main
 */
int main() {
  typical_usage();
  init_example();
  order_by_lambda();
  order_by_function_object();
  order_by_user_defined_object();
  return 0;
}
