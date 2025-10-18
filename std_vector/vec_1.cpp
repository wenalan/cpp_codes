#include <iostream>
#include <utility>
#include <memory>
#include <concepts>
#include <cassert>
#include <iterator>

template<typename T>
class my_vec {
    T* data_;
    size_t size_;
    size_t cap_;

public:
    my_vec() : data_(nullptr), size_(0), cap_(0) {}
    my_vec(std::initializer_list<T> il) : data_(nullptr), size_(il.size()), cap_(il.size()) {
        data_ = new T(il.size());
        std::copy(il.begin(), il.end(), data_);
    }
    ~my_vec() { delete[] data_; }

    my_vec(const my_vec& o) : data_(nullptr), size_(o.size_), cap_(o.cap_) {
        T* new_data = new T[o.cap_];
        // for (size_t i=0; i<o.size_; ++i) {
        //     new_data[i] = o.data_[i];
        // }
        std::copy(o.data_, o.data_+o.size_, new_data);
        data_ = new_data;
    }
    my_vec& operator=(const my_vec& o) {
        if (this == &o) return *this;

        T* new_data = new T[o.cap_];
        // for (size_t i=0; i<o.size_; ++i) {
        //     new_data[i] = o.data_[i];
        // }
        std::copy(o.data_, o.data_+o.size_, new_data);

        delete[] data_;
        data_ = new_data;
        size_ = o.size_;
        cap_ = o.cap_;
        
        return *this;
    }

    my_vec(my_vec&& o) noexcept : data_(o.data_), size_(o.size_), cap_(o.cap_) {
        o.data_ = nullptr;
        o.size_ = 0;
        o.cap_ = 0;
    }
    my_vec& operator=(my_vec&& o) noexcept {
        if (this == &o) return *this;

        delete[] data_;
        data_ = o.data_;
        size_ = o.size_;
        cap_= o.cap_;
        o.data_ = nullptr;
        o.size_ = 0;
        o.cap_ = 0;

        return *this;
    }

    void reserve(size_t new_cap) {
        if (new_cap < cap_) return;

        T* new_data = new T[new_cap];
        // for (size_t i=0; i<size_; ++i) {
        //     new_data[i] = std::move(data_[i]);
        // }
        std::move(data_, data_+size_, new_data);

        delete[] data_;
        data_ = new_data;
        cap_ = new_cap;
    }

    void push_back(const T& v) {
        T tmp = v;
        if (size_ == cap_) reserve(cap_ ? cap_*2 : 1);
        data_[size_] = std::move(tmp);
        ++size_;
    }
    void push_back(T&& v) {
        T tmp = std::move(v);
        if (size_ == cap_) reserve(cap_ ? cap_*2 : 1);
        data_[size_] = std::move(tmp);
        ++size_;
    }
    void pop_back() { --size_; }

    template<typename... Args>
    void emplace_back(Args&&... args) {
        T tmp(std::forward<Args>(args)...);
        if (size_ == cap_) reserve(cap_ ? cap_*2 : 1);
        data_[size_] = std::move(tmp);
        ++size_;
    }

    T& operator[](size_t index) { return data_[index]; }
    const T& operator[](size_t index) const { return data_[index]; }
    bool operator==(const my_vec& o) const {
        // if (size_ != o.size_) return false;
        // for (size_t i=0; i<size_; ++i) {
        //     if (data_[i] != o.data_[i]) return false;
        // }
        return size_ == o.size_ && std::equal(data_, data_+size_, o.data_);
    }
    std::strong_ordering operator<=>(const my_vec& o) const {
        if (size_ != o.size_) return size_ <=> o.size_;
        for (size_t i=0; i<size_; ++i) {
            if (data_[i] != o.data_[i]) return data_[i] <=> o.data_[i];
        }
        return std::strong_ordering::equal;

    }

    size_t size() const noexcept { return size_; }
    size_t cap() const noexcept { return cap_; }
    bool empty() const noexcept { return size_ == 0; }

    using iterator = T*;
    iterator begin() { return data_; }
    iterator end() { return data_ + size_; }
    const iterator begin() const { return data_; }
    const iterator end() const { return data_ + size_; }
};

int main() {
  my_vec<int> v;

  for (int i=0; i<5; ++i) v.push_back(i);

  std::cout << "size " << v.size() << "\n";
  std::cout << "cap " << v.cap() << "\n";
  for (const auto & i : v) {
    std::cout << i << " ";
  }

  v.pop_back();
  std::cout << "\nsize " << v.size();
  std::cout << "\ncap " << v.cap();
  std::cout << std::endl;

  my_vec<int> v2;
  std::cout << "test move v2";
  std::cout << "\nsize " << v2.size();
  std::cout << "\ncap " << v2.cap();
  v2 = std::move(v);
  std::cout << "\nsize " << v2.size();
  std::cout << "\ncap " << v2.cap();
  std::cout << std::endl;

  my_vec<int> v3(std::move(v2));
  std::cout << "test move v3";
  std::cout << "\nsize " << v2.size();
  std::cout << "\ncap " << v2.cap();
  std::cout << "\nsize " << v3.size();
  std::cout << "\ncap " << v3.cap();
  std::cout << std::endl;

  my_vec<int> v4;
  std::cout << "test copy v4";
  std::cout << "\nsize " << v4.size();
  std::cout << "\ncap " << v4.cap();
  v4 = v3;
  std::cout << "\nsize " << v4.size();
  std::cout << "\ncap " << v4.cap();
  std::cout << std::endl;
  assert(v4 == v3);
  assert((v4 <=> v3) == std::strong_ordering::equal);
  v4.push_back(22);
  assert(v4 != v3);
  assert((v4 <=> v3) == std::strong_ordering::greater);

  my_vec<int> v5(v4);
  std::cout << "test copy v5";
  std::cout << "\nsize " << v5.size();
  std::cout << "\ncap " << v5.cap();
  std::cout << "\nsize " << v5.size();
  std::cout << "\ncap " << v5.cap();
  std::cout << std::endl;

  std::cout << "push back v1\n";
  v.push_back(int{11});
  const int x{12}; v.push_back(x);
  v.emplace_back(33);

  std::cout << "push back v2\n";
  my_vec<std::unique_ptr<int>> v_ptr;
  v_ptr.push_back(std::make_unique<int>(42));

  std::cout << "move unique ptr\n";
  my_vec<std::unique_ptr<int>> v_ptr2;
  v_ptr2 = std::move(v_ptr);

const my_vec<int> vv = {1, 2, 3};
for (auto it = vv.begin(); it != vv.end(); ++it) {
    std::cout << *it;   // ✅ OK
    // *it = 5;            // ❌ 编译错误（因为返回 const int*）
}
std::cout << std::endl;
  return 0;
}
