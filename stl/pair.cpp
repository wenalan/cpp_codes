#include <bits/stdc++.h>
using namespace std;


/******
 * quick reference:
 * 
 * make_pair
 * =
 * get
 * 
 */
void typical_usage() {
  cout << __func__ << endl;

  auto p = make_pair(1, 2);
  cout << p.first << " " << p.second << endl;
}


/******
 * move example
 * with overload << for pair and vector
 */
template<class Os, class T>
Os& operator<<(Os& os, const std::vector<T>& v)
{
    os << '{';
    for (std::size_t t = 0; t != v.size(); ++t)
        os << v[t] << (t + 1 < v.size() ? ", " : "");
    return os << '}';
}
 
template<class Os, class U1, class U2>
Os& operator<<(Os& os, const std::pair<U1, U2>& pair)
{
    return os << '{' << pair.first << ", " << pair.second << '}';
}

void move_example() {
  cout << __func__ << endl;

  pair<vector<int>, int> p{{5, 6}, 7};
  cout << setw(23)
            << "before move "
            << "p: " << p << '\n';

  pair<vector<int>, int> q = make_pair(vector<int>{1, 2}, 3);
  p = move(q);
  cout << setw(23)
            << "after move "
            << "p: " << p << "  q: " << q << '\n';

  cout << get<0>(p) << " " << get<int>(p) << endl;
}


/******
 * main
 */
int main() {
  typical_usage();
  move_example();
  return 0;
}
