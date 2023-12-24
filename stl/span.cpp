#include <bits/stdc++.h>
using namespace std;


/******
 * quick reference: // after c20
 *
 * ref = front()
 * ref = back()
 *
 * =     // shallow copy
 * at    // after c26
 *
 * size()
 * size_bytes()
 *
 * first(N)   // sub span composed by first N elements
 * last(N)    // sub span composed by last N elements
 * subspan(index, count)
 *
 * non-member
 * as_bytes
 * as_writeable_bytes
 */
void typical_usage() {
  cout << __func__ << endl;

  vector<int> v{1, 2, 3, 4, 5};
  span sp(v.begin(), 4);
  cout << sp[2] << " " << sp.front() << " " << sp.back() << endl;
  cout << sp.size() << " " << sp.size_bytes() << endl;
 
  for (int i=0; i<sp.size(); ++i) {
    cout << sp[i] << " ";
  }
  cout << endl;
}


/******
 * initialization
 */
void init_example() {
  cout << __func__ << endl;

  auto print = [](const auto& sp){
    for (auto const& e : sp) {
      cout << e << " ";
    }
    cout << endl;
  };

  span<const int> sp{{1, 2, 3, 4}};
  print(sp);

  vector<int> v{1, 2, 3, 4, 5};
  span sp1(v.begin(), v.end());
  print(sp1);
  
  print(sp1.first<3>());
  print(sp1.first(3));

  print(sp1.last<3>());
  print(sp1.last(3));

  print(sp1.subspan<1, 3>());
  print(sp1.subspan(1, 3));
}


/******
 * shallow copy
 */
void shallow_copy() {
  cout << __func__ << endl;
  
  vector<int> v{1, 2, 3, 4, 5};
  span sp1(v.begin(), v.end());
  span sp2 = sp1;

  cout << sp1[1] << " " << sp2[1] << endl;
  sp2[1] = 42;
  cout << sp1[1] << " " << sp2[1] << endl;
}


/******
 * main
 */
int main() {
  typical_usage();
  init_example();
  shallow_copy();
  return 0;
}
