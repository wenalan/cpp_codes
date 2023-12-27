#include <bits/stdc++.h>
using namespace std;


/* notes:
 *
 * 1) Unary right fold (E op ...) becomes (E1 op (... op (EN-1 op EN)))
 * 2) Unary left fold (... op E) becomes (((E1 op E2) op ...) op EN)
 * 3) Binary right fold (E op ... op I) becomes (E1 op (... op (EN−1 op (EN op I))))
 * 4) Binary left fold (I op ... op E) becomes ((((I op E1) op E2) op ...) op EN)
 *
 * 1) don't forget the parenthesis outside
 * 2) if operator is ',' then parenthesis does not change the order of calculation
 * 3) E always numbered from 1 to N, from left to right
 * 4) ... and I, marks the position of inner parenthesis
 *
 * operator | value when pack is empty
 *    &&    |   true
 *    ||    |   false
 *    ,     |   void()
 *
 * used in lambda
 *  void foo(Args... args) {
 *    [...xs=args]{
 *        bar(xs...);             // xs is an init-capture pack
 *    };
 *  }
 *
 */


/*******
 * print args
 */
template <typename... Args>
void print_args(Args&&... args) {
    (cout << ... << args) << endl;
}

template<typename T, typename... Args>
void push_back_vec(std::vector<T>& v, Args&&... args) {
    static_assert((std::is_constructible_v<T, Args&&> && ...));
    (v.push_back(std::forward<Args>(args)), ...);
    // (..., v.push_back(std::forward<Args>(args))); // same border
}


/*******
 * print strings
 * https://www.scs.stanford.edu/~dm/blog/param-pack.html
 */
void print_strings(std::convertible_to<std::string_view> auto&& ...s)
{
  for (auto v : std::initializer_list<std::string_view>{ s... })
    std::cout << v << " ";
  std::cout << std::endl;
}


/*******
 * init capture packs
 */
template<std::convertible_to<std::string> ...T>
auto make_prefixer(T&& ...args) {
  using namespace std::string_literals;
  return [...p=std::string(std::forward<T>(args))](std::string msg) {
    // binary right fold over +
    return ((p + ": "s + msg + "\n"s) + ... + ""s);
  };
}

void init_capture_packs() {
  auto p = make_prefixer("BEGIN", "END");
  std::cout << p("message");
  // prints:
  // BEGIN: message
  // END: message
}

/*******
 * convert to upper case
 */
void dump_msg(auto&&... args) {
    (cout << ... << args) << endl;
}

template<std::same_as<char> ...C>
void convert_to_upper_case(C...c) {
  std::tuple<C...> tpl(c...);

  const char msg[] = { C(std::toupper(c))..., '\0' };
  dump_msg(msg, c...);
}


/*******
 * nested
 */
constexpr int sum(std::convertible_to<int> auto ...il) {
  int r = 0;
  for (int i : { int(il)... })
    r += i;
  return r;
}

template<int ...N>
struct Nested {
  static constexpr int nested_sum(auto ...v) {
    return sum(sum(N..., v)...);
  }
};


/*******
 * overloaded lambda
 */
// https://www.fluentcpp.com/2021/03/19/what-c-fold-expressions-can-bring-to-your-code/
struct Employee {
    int id;
    string name;

    int getId() { return id; }
};

/*
struct CompareWithId
{
    bool operator()(Employee const& employee, int id)
    {
        return employee.getId() < id;
    }
    bool operator()(int id, Employee const& employee)
    {
        return id < employee.getId();
    }
};
*/

template<typename... Lambdas>
struct overloaded : public Lambdas... {
    // ? still works if uncomments line 55 and line 59 !
    // explicit overloaded(Lambdas... lambdas) : Lambdas(lambdas)... {}
    using Lambdas::operator()...;
};

// template<typename... Lambdas> overloaded(Lambdas...) -> overloaded<Lambdas...>;

auto compareWithId = overloaded {
    [](auto&& employee, int id) { return employee.getId() < id; },
    [](int id, auto&& employee) { return id < employee.getId(); }
};

void overload_test() {
    Employee employee{6, "abc"};

    cout << compareWithId(employee, 8) << endl;
    cout << compareWithId(employee, 2) << endl;
    cout << compareWithId(1, employee) << endl;
    cout << compareWithId(9, employee) << endl;
}


/*******
 * recursing over argument list
 */
inline void
printall()
{
}

void
printall(const auto &first, const auto &...rest)
{
  std::cout << first;
  printall(rest...);
}

// after C++17
void
printall2(const auto &...args)
{
  // binary left fold
  (std::cout << ... << args);
}


/*******
 * recursing over template parameters
 */
template<char ...Cs>
struct string_holder {
  static constexpr std::size_t len = sizeof...(Cs);
  static constexpr char value[] = { Cs..., '\0' };
  constexpr operator const char *() const { return value; }
  constexpr operator std::string() const { return { value, len }; }
};

template<size_t N, char...Cs>
consteval auto
index_string()
{
  if constexpr (N < 10)
    return string_holder<N+'0', Cs...>{};
  else
    return index_string<N/10, (N%10)+'0', Cs...>();
}

// "10"
constinit const char *ten = index_string<10>();

template<char ...Out>
consteval auto
add_commas(string_holder<>, string_holder<Out...> out)
{
  return out;
}

template<char In0, char ...InRest, char ...Out>
consteval auto
add_commas(string_holder<In0, InRest...>, string_holder<Out...> = {})
{
  if constexpr (sizeof...(InRest) % 3 == 0 && sizeof...(InRest) > 0)
    return add_commas(string_holder<InRest...>{},
                      string_holder<Out..., In0, ','>{});
  else
    return add_commas(string_holder<InRest...>{},
                      string_holder<Out..., In0>{});
}

// "1,000,000"
constinit const char *million = add_commas(index_string<1'000'000>());


/*******
 * comma fold
 */
template<typename T, typename ...E>
void multi_insert(T &t, E&&...e) {
  // cast the expression to void to avoid any strange behavior
  // in cases where the program overloads operator,
  // unary right fold over comma
  (void(t.insert(std::forward<E>(e))), ...);
}

int comma_fold() {
  std::set<int> s;
  multi_insert(s, 1, 4, 7, 10);
  for (auto i : s)
    std::cout << i << " ";
  std::cout << std::endl;
  // prints:
  // 1 4 7 10
}


/*******
 * main
 */
int main() {
    print_args(42, "abc", 3.14);
    print_strings("one", std::string{"two"});

    static_assert(Nested<1>::nested_sum(100, 200) == 302);
    // Equivalent to:  sum(sum(1, 100),sum(1, 200)) == 302

    init_capture_packs();

    std::cout << ten << endl;
    std::cout << million << endl;

    convert_to_upper_case('a', 'b');

    vector<int> v{};
    push_back_vec(v, 1, 2, 9);
    for (int i : v)
        std::cout << i << ' ';
    std::cout << '\n';

    overload_test();

    comma_fold();
}
