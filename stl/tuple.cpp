#include <bits/stdc++.h>
using namespace std;


/******
 * quick reference:
 * 
 * =
 *
 * non-member
 * get
 * tie
 * make_tuple
 * forward_as_tuple()
 * tuple_cat(...)
 */
void typical_usage() {
  cout << forward_as_tuple << tuple_cat_example
  cout << __func__ << endl;

  auto tp = make_tuple("abc", 42, 3.14);
  cout << get<0>(tp) << " " << get<1>(tp) << " " << get<double>(tp) << endl;
}


/******
 * advanced example
 */
template<class Os, class T>
Os& operator<<(Os& os, const std::set<T>& st)
{
    os << '{';
    for (auto e : st)
        os << e << " ";
    return os << '}';
}

template<class Os, class U1, class U2>
Os& operator<<(Os& os, const std::tuple<U1, U2>& tp)
{
    return os << '{' << get<0>(tp) << ", " << get<1>(tp) << '}';
}

void advanced_example() {
  cout << __func__ << endl;

  // sort by first col, then second
  std::set<tuple<int, int>> st;
  bool is_inserted;
  std::tie(std::ignore, is_inserted) = st.insert({1, 2});
  assert(is_inserted);

  auto [_, inserted] = st.insert({5, 2});
  assert(inserted);

  auto [_1, inserted1] = st.insert({1, 1});
  assert(inserted);

  cout << st << endl;
}

/******
 * forward_as_tuple
 *
 * Constructs a tuple of references to the arguments in args suitable for forwarding as 
 * an argument to a function. The tuple has rvalue reference data members when rvalues 
 * are used as arguments, and otherwise has lvalue reference data members.
 *
 * If the arguments are temporaries, forward_as_tuple does not extend their lifetime;
 * they have to be used before the end of the full expression.
 */
void forward_as_tuple_example() {
  std::map<std::string, std::string> m;

  m.emplace(std::piecewise_construct,
            std::forward_as_tuple(2, 'a'),
            std::forward_as_tuple(4, 'b'));
  // m["aa"] = "bbbb"

  // The following is an error: it produces a
  // std::tuple<int&&, char&&> holding two dangling references.
  //
  // auto t = std::forward_as_tuple(20, 'a');
  // m.emplace(std::piecewise_construct, std::forward_as_tuple(10), t);
}


/******
 * tuple_cat
 */
template<class Tuple, std::size_t N>
struct TuplePrinter
{
    static void print(const Tuple& t)
    {
        TuplePrinter<Tuple, N - 1>::print(t);
        std::cout << ", " << std::get<N-1>(t);
    }
};
 
template<class Tuple>
struct TuplePrinter<Tuple, 1>
{
    static void print(const Tuple& t)
    {
        std::cout << std::get<0>(t);
    }
};

template<typename... Args, std::enable_if_t<sizeof...(Args) == 0, int> = 0>
void print(const std::tuple<Args...>& t)
{
    std::cout << "()\n";
}
 
template<typename... Args, std::enable_if_t<sizeof...(Args) != 0, int> = 0>
void print(const std::tuple<Args...>& t)
{
  std::cout << "(";
  TuplePrinter<decltype(t), sizeof...(Args)>::print(t);
  std::cout << ")\n";
}

void tuple_cat_example() {
  std::tuple<int, std::string, float> t1(10, "Test", 3.14);
  int n = 7;
  auto t2 = std::tuple_cat(t1, std::make_tuple("Foo", "bar"), t1, std::tie(n));
  n = 42;
  print(t2);
}


/*
https://www.murrayc.com/permalink/2015/12/05/modern-c-variadic-template-parameters-and-tuples/

https://devblogs.microsoft.com/oldnewthing/20200529-00/?p=103810/
template<typename... Args>
struct Traits
{
    using Tuple = std::tuple<Args...>;
    static constexpr auto Size = sizeof...(Args);
    template <std::size_t N>
    using Nth = typename std::tuple_element<N, Tuple>::type;
    using First = Nth<0>;
    using Last  = Nth<Size - 1>;
};
*/

/******
 * main
 */
int main() {
  typical_usage();
  advanced_example();
  forward_as_tuple_example();
  tuple_cat_example();
  return 0;
}
