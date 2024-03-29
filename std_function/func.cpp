#include <functional>
#include <iostream>

int getOne(int a) {
    return 1;
}

struct getTwo {
    getTwo() {}
    int operator()(int a) {
        return 2;
    }
};

// 默认特化不实现
template < typename T >
class Function;

template < typename Ret, typename Args0 >
class Function< Ret(Args0) > {
    //构造虚基类以存储任意可调用对象的指针
    struct callable_base {
        virtual Ret operator()(Args0 a0) = 0;
        virtual struct callable_base *copy() const = 0;
        virtual ~callable_base(){};
    };
    struct callable_base *base;

    template < typename T >
    struct callable_derived : public callable_base {
        T f;
        callable_derived(T functor) : f(functor) {}
        Ret operator()(Args0 a0) {
            return f(a0);
        }
        struct callable_base *copy() const {
            return new callable_derived< T >(f);
        }
    };

public:
    template < typename T >
    Function(T functor) : base(new callable_derived< T >(functor)) {}
    Function() : base(nullptr) {}
    // 实际调用存储的函数
    Ret operator()(Args0 arg0) {
        return (*base)(arg0);
    }

    Function(const Function &f) {
        std::cout << "copy construct" << std::endl;
        base = f.base->copy();
    }
    Function &operator=(const Function &f) {
        std::cout << "assign construct" << std::endl;
        delete base;
        base = f.base->copy();
        return *this;
    }

    ~Function() {
        std::cout << "delete current base callable object" << std::endl;
        delete base;
    }
};

int main() {
    // basic function
//    class Function< int(int) > getNumber(getOne);
//    std::cout
//    << getNumber(3)
//    << std::endl;
//
    // class which override operator()
//    class Function< int(int) > getNumber2(getTwo{});
//    std::cout << getNumber2(2) << std::endl;

    class Function< int(int) > getNumber2([](int x){return 42;});
    std::cout << getNumber2(2) << std::endl;

    // class Function< int(int) > getNumber3 = getNumber2;
    // getNumber3 = getNumber;
    return 0;
}
