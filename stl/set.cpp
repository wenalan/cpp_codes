#include <bits/stdc++.h>
using namespace std;


/******
 * quick reference:
 *
 * [it, inserted] = insert(val)
 * n_removed = erase(val)  // after c23, support heterogeneous lookup
 * n = count()
 * clear()
 *
 * bool contains(val)      // after c20, support heterogeneous lookup
 * it_after = erase(it)
 * [it, inserted] = emplace(...)
 * it = emplace_hint(hint, ...)
 *
 * merge(src_set) // merge src_set into current set
 *                // valid elements are removed from src_set
 *
 * // after c14, support heterogeneous lookup
 * [it>=, it>] = equal_range(val)
 * [it>=] = lower_bound(val)
 * [it>] = upper_bound(val)
 */
void typical_usage() {
  set<int> st;
  auto [it, inserted] = st.insert(42);
  cout << *it << " " << inserted << endl;
  auto n_removed = st.erase(42);
  cout << n_removed << endl;
  cout << st.count(42) << endl;
}


/******
 * initialization
 */
void init_example() {
  cout << "init_example" << endl;

  // init from initilizer list
  set<int> st1{1, 2, 3};

  auto [first_it, second_it] = st1.equal_range(2);
  cout << *first_it << " " << *second_it << endl;

  // init from vector iterator
  vector<int> vec{1, 4, 5};
  set<int, greater<>> st2{ vec.begin(), vec.end() };
  st2.insert({ 1, 10, 12 });  // add elements 1, 10, 12
  cout << st2.size() << endl;

  st2.merge(st1);  // merged elements are removed from st1
  cout << st2.size() << endl;

  st2.clear();
}


/******
 * use pair in unordered_set
 */
// by lambda
void pair_in_unordered_set_lambda() {
	auto pair_hash = [](const std::pair<int, int> &p) {
			return ((size_t)p.first << 32) | p.second;
	};

	// it is possible to assign different algorithms for different sets
	unordered_set<pair<int, int>, decltype(pair_hash)> visited_set;
	unordered_map<pair<int, int>, int, decltype(pair_hash)> visited_map;
}

// by function object
struct pair_hash {
    std::size_t operator()(const std::pair<int, int> &p) const {
        return ((size_t)p.first << 32) | p.second;
    }
};

void pair_in_unordered_set_function_object() {
	// it is possible to assign different algorithms for different sets
  unordered_set<pair<int, int>, pair_hash> visited_set;
  unordered_map<pair<int, int>, int, pair_hash> visited_map;
}

// by global template class specialization
template<>
struct std::hash<pair<int,int>> {
  std::size_t operator()(pair<int,int> const& p) const noexcept {
    return ((size_t)p.first << 32) | p.second;
  }
};

void pair_in_unordered_set_global() {
  // all sets use the same hash algorithm
  unordered_set<pair<int, int>> visited_set;
  unordered_map<pair<int, int>, int> visited_map;
}

/******
 * hint example
 */
struct obj {
  obj(int a) : x(a) {} // non-explicit, to support initialization below
  int x;

  // operator < is required for set
  friend bool operator<(const obj& a, const obj& b) {
    return a.x < b.x;
  }
};

void emplace_hint_example() {
  set<obj> s{2, 5, 9};
  // element should be inserted right before or after the hint
  // or exactly equal to the hint position
  // otherwise, it fall back to the normal emplace
  auto it = s.emplace_hint(s.begin(), 3); // emplace with hint
  it = s.emplace_hint(s.begin(), 7); // fall back to normal emplace
}

void insert_example() {
  set<int> s{2, 5, 9};
  // element should be inserted right before or after the position
  // or exactly equal to the position
  // otherwise, it fall back to the normal insertion
  auto it = s.insert(s.begin(), 3); // inserted with hint
  it = s.insert(s.begin(), 7); // fall back to normal insertion
}

/******
 * standard algorithm
 */
template <typename OS, typename T>
OS& operator<<(OS& os, const T& v) {
  auto sep = false;
  for (const auto& e : v) {
    os << (sep ? ", " : (sep=true, "")) << e;
  }
  return os;
}

void algorithm_example() {
  cout << "algorithm example" << endl;

  // must be sorted elements
  vector<int> v1{1, 2, 3};
  vector<int> v2{3, 4, 5};
  vector<int> v3;

  // result is {3}
  set_intersection(v1.begin(), v1.end(),
                   v2.begin(), v2.end(),
                   back_inserter(v3));
  cout << v3 << endl;
  v3.clear();

  // result is {1, 2, 4, 5}
  set_symmetric_difference(v1.begin(), v1.end(),
                           v2.begin(), v2.end(),
                           back_inserter(v3));
  cout << v3 << endl;
  v3.clear();

  // result is {1, 2}, sth like v1 - v2
  set_difference(v1.begin(), v1.end(),
                 v2.begin(), v2.end(),
                 back_inserter(v3));
  cout << v3 << endl;
  v3.clear();

  // result is {1, 2, 3, 4, 5}, no duplicated items
  set_union(v1.begin(), v1.end(),
            v2.begin(), v2.end(),
            back_inserter(v3));
  cout << v3 << endl;
  v3.clear();

  // result is {1, 2, 3, 3, 4, 5}, contains duplicated items
  merge(v1.begin(), v1.end(),
        v2.begin(), v2.end(),
        back_inserter(v3));
  cout << v3 << endl;
  v3.clear();
}


/******
 * main
 */
int main() {
  typical_usage();
  init_example();
  pair_in_unordered_set_lambda();
  pair_in_unordered_set_function_object();
  pair_in_unordered_set_global();
  emplace_hint_example();
  insert_example();
  algorithm_example();
  return 0;
}
