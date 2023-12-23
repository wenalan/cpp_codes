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
  cout << endl << "***************" << endl;
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
  cout << endl << "***************" << endl;
  cout << __func__ << endl;

  // init from initilizer list
  map<int, int> mp1{{1, 2}, {3, 4}, {9, 10}};
  map<pair<int, int>, int> mp2{{{1, 2}, 3}, {{4, 5}, 6}};

  auto [first_it, second_it] = mp1.equal_range(3);
  cout << first_it->first << " " << second_it->first << endl;

  // init from vector iterator
  vector<pair<int, int>> vec{{1, 2}, {3, 4}};
  map<int, int, greater<>> mp3{ vec.begin(), vec.end() };

  mp3.insert({{5, 6}, {7, 8}});
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
  cout << endl << "***************" << endl;
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
  cout << endl << "***************" << endl;
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
  cout << endl << "***************" << endl;
  cout << __func__ << endl;

  // all maps use the same hash algorithm
  unordered_map<pair<int, int>, int> visited_map;
}


/******
 * emplace hint example
 * with friend operator <
 */
struct obj {
  int x;

  obj(int a) : x(a) {} // non-explicit, to support initialization below

  // operator < is required for map
  friend bool operator<(const obj& a, const obj& b) {
    return a.x < b.x;
  }
};

void emplace_hint_example() {
  cout << endl << "***************" << endl;
  cout << __func__ << endl;

  map<obj, int> mp{{1, 2}, {5, 9}};
  // element should be inserted right before or after the hint
  // or exactly equal to the hint position
  // otherwise, it fall back to the normal emplace
  auto it = mp.emplace_hint(mp.begin(), 3, 4); // emplace with hint
  it = mp.emplace_hint(mp.begin(), obj{7}, 4); // fall back to normal emplace
}


/******
 * try emplace example
 */
void try_emplace_example() {
  cout << endl << "***************" << endl;
  cout << __func__ << endl;

  map<obj, int> mp{{1, 2}, {5, 9}};
  auto [inserted_it, inserted] = mp.try_emplace(10, 11);
  // element should be inserted right before or after the hint
  // or exactly equal to the hint position
  // otherwise, it fall back to the normal emplace
  auto it = mp.try_emplace(mp.begin(), 3, 4); // emplace with hint
  it = mp.try_emplace(mp.begin(), 7, 4); // fall back to normal emplace
}


/******
 * insert hint example
 * with lambda comparation
 */
void insert_example() {
  cout << endl << "***************" << endl;
  cout << __func__ << endl;

  struct obj {
    int x;
  };

  auto cmp = [](const auto& a, const auto& b) {
    return a.x < b.x;
  };

  map<obj, int, decltype(cmp)> mp{{obj{2}, 8}, {obj{5}, 3}, {obj{9}, 6}};
  // element should be inserted right before or after the position
  // or exactly equal to the position
  // otherwise, it fall back to the normal insertion
  auto it = mp.insert(mp.begin(), {obj{3}, 10}); // inserted with hint
  it = mp.insert(mp.begin(), {obj{7}, 2}); // fall back to normal insertion
}


/******
 * test for temporary object creation
 */
struct payload {
  static inline bool trace{false};
  int x;

  payload(int a) : x(a) { // non-explicit, to support initialization below
    if (!trace) return;
    cout << "constructor" << endl;
  }

  payload(const payload& other) : x(other.x) {
    cout << "copy constructor" << endl;
  }

  payload(payload&& other) noexcept : x(std::exchange(other.x, 0)) {
    cout << "move constructor" << endl;
  }

  ~payload() {
    if (!trace) return;
    cout << "destructor" << endl;
  }
};

void test_temporary_object_creation() {
  cout << endl << "***************" << endl;
  cout << __func__ << endl;

  map<int, payload> mp;

  payload::trace = true;
  // for emplace method
  // always construct the payload object
  // if the key is duplicated, the payload object is destroyed immediately
  cout << "emplace new element" << endl;
  auto [inserted_it, inserted] = mp.emplace(1, 3);
  cout << inserted_it->first << " " << inserted_it->second.x << " " << inserted << endl;

  cout << "emplace dup element" << endl;
  auto [inserted_it1, inserted1] = mp.emplace(1, 31);
  cout << inserted_it1->first << " " << inserted_it1->second.x << " " << inserted1 << endl;

  cout << "emplace_hint new element" << endl;
  auto inserted_it2 = mp.emplace_hint(mp.begin(), 2, 4);
  cout << inserted_it2->first << " " << inserted_it2->second.x << endl;

  cout << "emplace_hint dup element" << endl;
  auto inserted_it3 = mp.emplace_hint(mp.begin(), 2, 41);
  cout << inserted_it3->first << " " << inserted_it3->second.x << endl;

  // for try_emplace method
  // does not construct the payload object if key is duplicated
  // however, the key object is always constructed
  cout << "try_emplace new element" << endl;
  auto [inserted_it4, inserted4] = mp.try_emplace(3, 5);
  cout << inserted_it4->first << " " << inserted_it4->second.x << " " << inserted4 << endl;

  cout << "try_emplace dup element" << endl;
  auto [inserted_it5, inserted5] = mp.try_emplace(3, 51);
  cout << inserted_it5->first << " " << inserted_it5->second.x << " " << inserted5 << endl;

  cout << "try_emplace new element with hint" << endl;
  auto inserted_it6 = mp.try_emplace(mp.begin(), 4, 7);
  cout << inserted_it6->first << " " << inserted_it6->second.x << endl;

  cout << "try_emplace dup element with hint" << endl;
  auto inserted_it7 = mp.try_emplace(mp.begin(), 4, 71);
  cout << inserted_it7->first << " " << inserted_it7->second.x << endl;

  // for insert method
  // a temproray payload object is always constructed
  // if no dup key, the object is copied / moved into map, and destroy the temproray object
  // if the key is dup, the temproray object is destroyed immediately
  cout << "insert new element" << endl;
  auto [inserted_it8, inserted8] = mp.insert({5, 2});
  cout << inserted_it8->first << " " << inserted_it8->second.x << " " << inserted8 << endl;

  cout << "insert dup element" << endl;
  auto [inserted_it9, inserted9] = mp.insert({5, 21});
  cout << inserted_it9->first << " " << inserted_it9->second.x << " " << inserted9 << endl;

  cout << "insert new element with hint" << endl;
  auto inserted_it10 = mp.insert(mp.begin(), {6, 2});
  cout << inserted_it10->first << " " << inserted_it10->second.x << endl;

  cout << "insert dup element with hint" << endl;
  auto inserted_it11 = mp.insert(mp.begin(), {6, 21});
  cout << inserted_it11->first << " " << inserted_it11->second.x << endl;

  payload::trace = false;
}

/******
 * erase while looping
 */
void erase_while_looping() {
  cout << endl << "***************" << endl;
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
  try_emplace_example();
  insert_example();
  test_temporary_object_creation();
  erase_while_looping();
  return 0;
}
