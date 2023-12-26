#include <bits/stdc++.h>
using namespace std;


/******
 * quick reference:
 * 
 * =
 *
 * non-member
 * make_pair
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
 * piecewise_construct
 */
struct Foo
{
    Foo(std::tuple<int, float>)
    {
        std::cout << "Constructed a Foo from a tuple\n";
    }
 
    Foo(int, float)
    {
        std::cout << "Constructed a Foo from an int and a float\n";
    }
};

void piecewise_construct_example() {
    std::tuple<int, float> t(1, 3.14);
 
    std::cout << "Creating p1...\n";
    std::pair<Foo, Foo> p1(t, t);
 
    std::cout << "Creating p2...\n";
    std::pair<Foo, Foo> p2(std::piecewise_construct, t, t);


    std::map<std::string, std::string> m;
    m.emplace(std::piecewise_construct,
          std::forward_as_tuple("k"),
          std::forward_as_tuple(3, 'c'));
    // m[k] == 'ccc'
}


/******
 * main
 */
int main() {
  typical_usage();
  move_example();
  piecewise_construct_example();
  return 0;
}
