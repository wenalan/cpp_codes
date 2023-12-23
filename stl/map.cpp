#include <bits/stdc++.h>
using namespace std;


/******
 * notes:
 * 1) use rbegin() to access last element in map
 * 2) unordered_map provided begin() but not rbegin()
 */


/******
 * quick reference:
 *
 * map[key] = val
 * n_removed = erase(key)  // after c23, support heterogeneous lookup
 * n = count(key)          // after c14, support heterogeneous lookup
 * clear()
 *
 * it_after = erase(it)
 * [it, inserted] = insert(kv_pair)
 * it = insert(it_hint, kv_pair)
 * bool contains(key)      // after c20, support heterogeneous lookup
 *
 * // constructed obj destroyed immediately if dup
 * [it, inserted] = emplace(kv_pair...)
 * it = emplace_hint(it_hint, kv_pair...)
 *
 * // no obj construction if dup
 * [it, inserted] = try_emplace(key, val...)
 * it = try_emplace(it_hint, key, val...)
 *
 * merge(src_map) // merge src_map into current map
 *                // valid elements are removed from src_map
 *
 * // after c14, support heterogeneous lookup
 * [it>=, it>] = equal_range(key)
 * [it>=] = lower_bound(key)
 * [it>] = upper_bound(key)
 */
void typical_usage() {
  cout << __func__ << endl;

  map<int, int> mp;
  mp[42] = 17;
  auto n_removed = mp.erase(42);
  cout << n_removed << endl;
  cout << mp.count(42) << endl;
}


/******
 * initialization
 */
void init_example() {
  cout << __func__ << endl;

  // init from initilizer list
  map<int, int> mp1{{1, 2}, {3, 4}, {9, 10}};
  map<pair<int, int>, int> mp2{{{1, 2}, 3}, {{4, 5}, 6}};

  auto [first_it, second_it] = mp1.equal_range(3);
  cout << first_it->first << " " << second_it->first << endl;

  // init from vector iterator
  vector<pair<int, int>> vec{{1, 2}, {3, 4}};
  map<int, int, greater<>> mp3{ vec.begin(), vec.end() };

  mp3.insert({{5, 6}, {7, 8}});  // add elements 1, 10, 12
  cout << mp3.size() << endl;

  cout << mp1.size() << endl;
  mp3.merge(mp1);  // merged elements are removed from mp1
  cout << mp1.size() << endl;
  cout << mp3.size() << endl;

  mp3.clear();


  // with customized cmp lambda without capture
  struct Point {
    double x, y;
  };
  auto cmp = [](const auto& a, const auto& b) {
    return a.x < b.x;
  };
  map<Point, int, decltype(cmp)> mp4;


  // with customized cmp lambda with capture
  int dep = 3;
  auto cmp_dep = [dep](const auto& a, const auto& b) {
    return a.x < b.x;
  };
  // Since cmp_dep captured dep, it must provided as construtor param
  // if omitted, gcc reports the following error message
  // a lambda closure type has a deleted default constructor
  map<Point, int, decltype(cmp_dep)> mp5(cmp_dep);
}


/******
 * use pair in unordered_map
 */
// by lambda
void pair_in_unordered_map_lambda() {
  cout << __func__ << endl;

	auto pair_hash = [](const std::pair<int, int> &p) {
			return ((size_t)p.first << 32) | p.second;
	};

	// it is possible to assign different algorithms for different maps
	unordered_map<pair<int, int>, int, decltype(pair_hash)> visited_map;
}

// by function object
struct pair_hash {
    std::size_t operator()(const std::pair<int, int> &p) const {
        return ((size_t)p.first << 32) | p.second;
    }
};

void pair_in_unordered_map_function_object() {
  cout << __func__ << endl;

	// it is possible to assign different algorithms for different maps
  unordered_map<pair<int, int>, int, pair_hash> visited_map;
}

// by global template class specialization
template<>
struct std::hash<pair<int,int>> {
  std::size_t operator()(pair<int,int> const& p) const noexcept {
    return ((size_t)p.first << 32) | p.second;
  }
};

void pair_in_unordered_map_global() {
  cout << __func__ << endl;

  // all maps use the same hash algorithm
  unordered_map<pair<int, int>, int> visited_map;
}

/******
 * emplace hint example
 * with friend operator <
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
  cout << __func__ << endl;

  set<obj> s{2, 5, 9};
  // element should be inserted right before or after the hint
  // or exactly equal to the hint position
  // otherwise, it fall back to the normal emplace
  auto it = s.emplace_hint(s.begin(), 3); // emplace with hint
  it = s.emplace_hint(s.begin(), 7); // fall back to normal emplace
}

/******
 * insert hint example
 * with lambda comparation
 */
void insert_example() {
  cout << __func__ << endl;

  struct obj {
    int x;
  };

  auto cmp = [](const auto& a, const auto& b) {
    return a.x < b.x;
  };

  set<obj, decltype(cmp)> s{obj{2}, obj{5}, obj{9}};
  // element should be inserted right before or after the position
  // or exactly equal to the position
  // otherwise, it fall back to the normal insertion
  auto it = s.insert(s.begin(), obj{3}); // inserted with hint
  it = s.insert(s.begin(), obj{7}); // fall back to normal insertion
}

/******
 * erase while looping
 */
void erase_while_looping() {
  cout << __func__ << endl;

  map<int, int> mp1{{1, 2}, {3, 4}, {8, 10}};
  for (auto it = mp1.begin(); it != mp1.end(); ) {
    if (it->first % 2) {
      // does not support rbegin()
      // because erase() does not accept reverse iterator
      it = mp1.erase(it);
    } else {
      ++it;
    }
  }
  cout << mp1.size() << " " << mp1.count(8) << endl;
}

/******
 * main
 */
int main() {
  typical_usage();
  init_example();
  pair_in_unordered_map_lambda();
  pair_in_unordered_map_function_object();
  pair_in_unordered_map_global();
  emplace_hint_example();
  insert_example();
  erase_while_looping();
  return 0;
}
