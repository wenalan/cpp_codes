#include <iostream>


template<typename...T>
auto sumleft(T...s) {
	return(... + s);
}

template<typename...T>
decltype(auto) sumright(T...s) {
	return (s + ...);
}


template<typename...Args>
void put(Args...args) {
	((std::cout << args << '\t'), ...) <<'\n';
}

template<typename...Args>
void out(Args...args) {
	([](const auto& x) {
		std::cout << x << '\t';
		}(args),...);
	std::cout << '\n';
}


template<typename T>
void print(T t) {
	std::cout << t << '\n';
}

template<typename T, typename... Rest>
void print(T first, Rest...rest) {
	std::cout << first << "\t";
	print(rest...);
}


int sum() { return 0; }

template<typename T1, typename...T>
auto sum(T1 s, T...ts){
	std::cout << s << "\t";
	return s + sum(ts...);
}

int main() {
	print("Li");
	print(2, "Li");
	print(2, "LI", 80.5, 3, '6');
	std::cout << sum(1, 2) << '\n';
	std::cout << sum(1, 2, 3) << '\n';
	std::cout << sumleft(1, 2, 3, 4) << '\n';
	std::cout << sumright(1, 2, 3, 4) << '\n';
	put(1, 2, 3, 4);
	out(1, 2, 3, 4);
	return 0;
}

