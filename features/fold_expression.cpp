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
 */

template <typename... Args>
void print_args(Args&&... args) {
    (cout << ... << args) << endl;
}

template<typename T, typename... Args>
void push_back_vec(std::vector<T>& v, Args&&... args) {
    static_assert((std::is_constructible_v<T, Args&&> && ...));
    (v.push_back(std::forward<Args>(args)), ...);
    // (..., v.push_back(std::forward<Args>(args))); // same order
}

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
    // ? works if uncomments line 55 and line 59 ?
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

int main() {
    print_args(42, "abc", 3.14);

    vector<int> v{};
    push_back_vec(v, 1, 2, 9);
    for (int i : v)
        std::cout << i << ' ';
    std::cout << '\n';

    overload_test();
}
