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

void comma_fold() {
  std::set<int> s;
  multi_insert(s, 1, 4, 7, 10);
  for (auto i : s)
    std::cout << i << " ";
  std::cout << std::endl;
  // prints:
  // 1 4 7 10
}


/*******
 * short-circuiting
 */
template<typename T, typename F>
std::size_t tuple_find(const T &t, F &&f) {
  // apply every element of t to function f
  return std::apply([&f](const auto &...e) {
    std::size_t r = 0;
    // if e lesser or equal to -1, continue to eval ++r
    //   and continue to eval next element
    // if e greater than -1, than stop and return r immediately
    //   due to short ciruiting
    ((std::forward<F>(f)(e) || (++r, false)) || ...);
    return r;
  }, t);
}

void short_circuiting() {
  std::tuple t(-2, -1, 0U, 1UL, 2ULL);
  std::cout << tuple_find(t, [](auto i) {
    // note: 0U > -1 is false
    return std::cmp_greater(i, -1);
  }) << std::endl;
  // output the number of items lesser or equal to -1
  // before met the first item greater than -1
  // 2

  //std::tuple t(-2, -1, -4, 0U, -1);
  // output 3

  //std::tuple t(0, -2, -1, 0U, -1);
  // output 0
}


/*******
 * capture packs by lambda
 */
auto tuple_mult(auto scalar, auto tpl) {
  // deconstruct tuple into elements
  return apply([&scalar]<typename ...T>(T...t) {
      // reconstruct elements into tuple
      return std::tuple(T(scalar * t)...);
    }, tpl);
}

template<typename T>
auto tuple_add(const T &a, const T&b) {
  return [&a, &b]<std::size_t ...I>(std::index_sequence<I...>) {
    return std::tuple(get<I>(a) + get<I>(b)...);
  // make_index_sequence<N> generate 0 to N-1
  }(std::make_index_sequence<std::tuple_size_v<T>>{});
}

void capture_by_lambda() {
  auto t = std::tuple(1, 2U, 4.0);
  t = tuple_mult(2, t);
  std::cout << get<0>(t) << " "
            << get<1>(t) << " "
            << get<2>(t) << std::endl;
  // prints:
  // 2 4 8

  auto t1 = std::tuple(1, 2U, 4.0);
  t1 = tuple_add(t1, tuple_mult(10, t1));
  std::cout << get<0>(t1) << " "
            << get<1>(t1) << " "
            << get<2>(t1) << std::endl;
  // prints:
  // 11 22 44
}


/*******
 * capture packs in requires clauses
 */
constexpr int hexdigit(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  c |= 0x20;            // convert upper- to lower-case
  if (c >= 'a' && c <= 'f')
    return c - ('a' - 10);
  return -1;            // invalid
}

template<char ...C>
requires (sizeof...(C)%2 == 0 &&
          [](char zero, char x, auto ...rest) {
            return zero == '0' && x == 'x' && ((hexdigit(rest) != -1) && ...);
          }(C...))
constexpr std::string operator""_hex() {
  constexpr std::array digits{ C... };
  std::string result{};
  for (std::size_t i = 2; i < digits.size(); i += 2)
    result += char(hexdigit(digits[i])<<4 | hexdigit(digits[i+1]));
  return result;
}

template<char ...C>
requires (sizeof...(C)%2 == 0 &&
          [](char zero, char x, auto ...rest) {
            return zero == '0' && x == 'x' && ((hexdigit(rest) != -1) && ...);
          }(C...))
consteval auto
operator""_hex2()
{
  return []<std::size_t ...I>(std::index_sequence<I...>) {
    constexpr std::array digits{ C... };
    return string_holder<hexdigit(digits[2*I+2])<<4 |
                         hexdigit(digits[2*I+3])...>{};
  }(std::make_index_sequence<sizeof...(C)/2-1>{});
}

void capture_packs_in_requires_clauses() {
  std::cout << 0x48656c6c6f21_hex << std::endl;
  std::cout << 0x48656c6c6f21_hex2 << std::endl;
  // prints:
  // Hello!
}


/*******
 * decltype on lambda expressions
 */
namespace detail {

template<typename T> struct tuple_ptr_helper;

template<typename ...T>
struct tuple_ptr_helper<std::tuple<T...>> {
  using type = std::tuple<T*...>;
};

} // namespace detail

template<typename T> using tuple_ptrs =
  typename detail::tuple_ptr_helper<T>::type;

// C++20
template<typename T> using tuple_ptrs2 =
  decltype(std::apply([](auto ...t) { return std::tuple(&t...); },
                      std::declval<T>()));


/*******
 * multilambda
 */
template<typename ...L>
struct multilambda : L... {
  using L::operator()...;
  constexpr multilambda(L...lambda) : L(std::move(lambda))... {}
};

void multilambda_example() {
  using namespace std::string_literals;
  std::tuple t (1, true, "hello"s, 3.0);
  constexpr multilambda action {
    [](int i) { std::cout << i << std::endl; },
    [](double d) { std::cout << d << std::endl; },
    [](bool b) { std::cout << (b ? "yes\n" : "no\n"); },
    [](std::string s) { std::cout << s.size() << " bytes\n"; },
  };
  apply([action](auto ...v) {
    (action(v), ...);  // unary right fold
  }, t);
  // prints:
  // 1
  // yes
  // 5 bytes
  // 3
}


/*******
 * multilambda2
 */
template<typename ...Lambdas>
struct multilambda2 : Lambdas... {
  using Lambdas::operator()...;
};
template<typename ...Lambdas>
multilambda2(Lambdas...) -> multilambda2<Lambdas...>;


template<typename T, template<typename...> typename Tmpl>
concept is_template = decltype(multilambda2{
    []<typename ...U>(const Tmpl<U...> &) { return std::true_type{}; },
    [](const auto &) { return std::false_type{}; },
  }(std::declval<T>()))::value;
// lambda accept input param declval<T>()
// if it matches function signature Tmpl<U...>
//    then return true_type
// otherwise return false_type

// the lambda above is actually return true_type::value
static_assert(decltype([](){ return std::true_type{};}())::value);
static_assert(std::true_type::value);

static_assert(is_template<std::tuple<long, long>, std::tuple>);
static_assert(is_template<const std::tuple<int, long> &, std::tuple>);
static_assert(!is_template<std::tuple<int, long>, std::variant>);


/*******
 * recursive types
 */
template<typename ...T> struct HList;

template<>
struct HList<> {
  static constexpr std::size_t len = 0;
};

template<typename T0, typename ...TRest>
struct HList<T0, TRest...> : HList<TRest...> {
  using head_type = T0;
  using tail_type = HList<TRest...>;

  static constexpr std::size_t len = 1 + sizeof...(TRest);
  [[no_unique_address]] head_type value_{};

  constexpr HList() = default;
  template<typename U0, typename ...URest>
  constexpr HList(U0 &&u0, URest &&...urest)
    : tail_type(std::forward<URest>(urest)...),
      value_(std::forward<U0>(u0)) {}

  head_type &head() & { return value_; }
  const head_type &head() const& { return value_; }
  head_type &&head() && { return value_; }

  tail_type &tail() & { return *this; }
  const tail_type &tail() const& { return *this; }
  tail_type &&tail() && { return *this; }
};

// User-defined class template argument deduction guide:
template<typename ...T> HList(T...) -> HList<T...>;

template<std::size_t N> struct dummy{};
// due to [[no_unique_address]], different type can reuse the same address
// thus the size of HList is 1
static_assert(sizeof(HList<dummy<0>, dummy<1>, dummy<2>>) == 1);
// [[no_unique_address]] does not work on the same type
// thus the size of HList is still 3
static_assert(sizeof(HList<dummy<0>, dummy<0>, dummy<0>>) == 3);

static_assert(HList<dummy<0>, dummy<0>, dummy<1>, dummy<0>>::len == 4);


/*******
 * my apply implementation
 */
template<std::size_t N, is_template<HList> HL>
requires (N <= std::remove_cvref_t<HL>::len)
inline decltype(auto)
drop(HL &&hl)
{
  if constexpr (N)
    return drop<N-1>(std::forward<HL>(hl).tail());
  else
    return std::forward<HL>(hl);
}

template<std::size_t N, is_template<HList> HL>
requires (N < std::remove_cvref_t<HL>::len)
inline decltype(auto)
get(HL &&hl)
{
  return drop<N>(std::forward<HL>(hl)).head();
}

template<typename F, is_template<HList> HL>
decltype(auto)
my_apply(F &&f, HL &&hl)
{
  [&f,&hl]<std::size_t ...I>(std::index_sequence<I...>) -> decltype(auto) {
    return std::forward<F>(f)(get<I>(std::forward<HL>(hl))...);
  }(std::make_index_sequence<std::remove_cvref_t<HL>::len>{});
}

struct A {
    int a;
};

void my_apply_example() {
  HList hl{A{0}, A{1}, A{2}};
  my_apply([]<typename ...T>(T... t){
      (cout << ... << (to_string(t.a) + " "))  << "end of line" << endl;
  }, hl);
}

/*******
 * homogeneous param packs
 * check homogeneous_param.cpp for more details
 */
struct Obj {
    Obj() = default;
    Obj (int x) : val(x) {}
    int val = 0;
    void use() {
        ++val;
        cout << "inside " << val << endl;
    }
};

template<typename Want, typename Have>
inline std::conditional_t<std::is_same_v<Want, Have>, Want &&, Want>
// if Have is Obj, then return Want&&
//    actual type depends lvalue or rvalue
// otherwise, return Want, need convert construction
local_copy(Have &in)
{
  return static_cast<Have&&>(in);
}

template<std::convertible_to<Obj> ...T>
void
good1(T&&...t)
{
  // Unary fold over comma operator
  (local_copy<Obj, T>(t).use(), ...);
}

// Another way to do it
template<std::convertible_to<Obj> ...T>
void
good2(T&&...t)
{
  auto use = []<typename U>(U &&arg) {
    decltype(auto) o = local_copy<Obj, U>(arg);
    o.use();
  };
  (use(std::forward<T>(t)), ...);
}

void homogeneous_param_packs()
{
    Obj a;
    a.val = 42;

    static_assert(std::is_same_v<Obj, decltype(a)> == true);
    static_assert(std::is_same_v<Obj, decltype(Obj{3})> == true);
    static_assert(std::is_same_v<Obj, decltype(3)> == false);

    good1(a);       // for existing lvalue obj, make a copy
    good2(a);

    good1(Obj{42}); // for rvalue object, move it
    good2(Obj{42});

    good1(38);      // for covertible obj, construct it
    good2(38);

    cout << "out " << a.val << endl;
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

    short_circuiting();
    capture_by_lambda();
    capture_packs_in_requires_clauses();

    static_assert(std::is_same_v<tuple_ptrs<std::tuple<int, char>>,
                                 std::tuple<int*, char*>>);
    static_assert(std::is_same_v<tuple_ptrs2<std::tuple<int, char>>,
                                 std::tuple<int*, char*>>);

    multilambda_example();

    my_apply_example();

    homogeneous_param_packs();
}
