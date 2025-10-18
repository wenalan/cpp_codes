#include <iostream>
#include <utility>
#include <memory>
#include <concepts>
#include <cassert>

// Invariants / Contracts
// 2) size_ <= cap_
// 2) 如果 cap_ == 0 则 size_ == 0
// 3) 索引区间：
//    - [0, size_)   为“已使用区”，对象处于已构造且可读写状态
//    - [size_, cap_)为“未使用区”，但由于 new T[]，这些对象也已默认构造
// 4) 拥有唯一所有权：data_ 仅由此 my_vec 管理；移动后源对象置为 {nullptr,0,0}
// 5) reserve(new_cap) 仅“增容”，不改变 size_，会使所有指针/引用/迭代器失效
// 6) pop_back 的前置条件：size_ > 0（与 std::vector 一样，空时调用是 UB；可用 assert 捕获）
//
// Type requirements
// - 需要 T 是 default-initializable（因 new T[]）
// - 复制构造/赋值仅当 T 可拷贝（用 concepts 约束）
// - 移动构造/赋值总是可用（与 T 无关）
//
// Semantics deviation vs std::vector
// - “延迟析构”策略：不在 pop_back() 析构元素，资源释放延迟到重分配或析构时由 delete[] 统一完成
//   * 影：对管理稀缺资源的 T（文件/锁/FD），资源不会在 pop_back 立即释放
//   * 这是有意取舍；若需要与 std::vector 等价的“即时析构”，必须改为未初始化存储 + placement new

template<typename T>
requires std::default_initializable<T>
class my_vec {
  T* data_;
  size_t size_;
  size_t cap_;

public:
  my_vec() : data_(nullptr), size_(0), cap_(0) {}
  ~my_vec() { delete[] data_; }

  my_vec(std::initializer_list<T> il)
      : data_(new T[il.size()]), size_(il.size()), cap_(il.size()) {
      std::copy(il.begin(), il.end(), data_);
  }

  my_vec(const my_vec& o) requires (!std::is_copy_assignable_v<T>) = delete;
  my_vec(const my_vec& o) requires std::is_copy_assignable_v<T>
  : data_(nullptr), size_(o.size_), cap_(o.cap_) {
    T* new_data = new T[o.cap_];
    try {
      for (size_t i=0; i<o.size_; ++i) {
        new_data[i] = o.data_[i]; // copy到新的obj
      }
    } catch (...) {
      delete[] new_data;
      throw;
    }
    data_ = new_data;
  }

  my_vec& operator= (const my_vec& o) requires (!std::is_copy_assignable_v<T>) = delete;
  my_vec& operator= (const my_vec& o) requires std::is_copy_assignable_v<T> {
    if (this == &o) return *this;

    T* new_data = new T[o.cap_];
    try {
      for (size_t i=0; i<o.size_; ++i) {
        new_data[i] = o.data_[i]; // copy到新的obj
      }
    } catch (...) {
      delete[] new_data;
      throw;
    }
    delete[] data_;  // 删除旧对象
    data_ = new_data;
    size_ = o.size_;
    cap_ = o.cap_;
    return *this;
  }

  my_vec(my_vec&& o) noexcept
  : data_(o.data_), size_(o.size_), cap_(o.cap_) {
    o.data_ = nullptr; // 置空之前对象
    o.size_ = 0;
    o.cap_ = 0;
  }

  my_vec& operator= (my_vec&& o) noexcept {
    if (this == &o) return *this;

    delete[] data_; // 删除旧对象

    data_ = o.data_; // 移动
    size_ = o.size_;
    cap_ = o.cap_;
    o.data_ = nullptr;  // 置空之前对象
    o.size_ = 0;
    o.cap_ = 0;

    return *this;
  }

  void reserve(size_t new_cap) {
    if (new_cap <= cap_) return;

    T* new_data = new T[new_cap];
    try {
      for (size_t i=0; i<size_; ++i) {
        // new_data[i] = std::move_if_noexcept(data_[i]);
        // 上面是检查构造函数是否抛异常，下面是看赋值函数
        if constexpr (std::is_nothrow_move_assignable_v<T> || !std::is_copy_assignable_v<T>) {
          new_data[i] = std::move(data_[i]);
        } else {
          new_data[i] = data_[i];
        }
      }
    } catch (...) {
      delete[] new_data;
      throw;
    }
    delete[] data_;
    data_ = new_data;
    cap_ = new_cap;
  }

  void push_back(const T& value) {
    T tmp = value;
    if (size_ == cap_) reserve(cap_ ? cap_ * 2 : 1);
    data_[size_] = std::move(tmp);
    ++size_;
  }

  void push_back(T&& value) {
    T tmp = std::move(value);
    if (size_ == cap_) reserve(cap_ ? cap_ * 2 : 1);
    data_[size_] = std::move(tmp);
    ++size_;
  }

  void pop_back() {
    assert(size_ > 0);
    --size_;
  }

  template<typename... Args>
  void emplace_back(Args&&... args) {
    T tmp(std::forward<Args>(args)...);
    if (size_ == cap_) reserve(cap_ ? cap_*2 : 1);
    data_[size_] = std::move(tmp);
    ++size_;
  }

  T& operator[](size_t index) {
    return data_[index];
  }

  const T& operator[](size_t index) const {
    return data_[index];
  }

  bool operator==(const my_vec& o) const {
    if (size_ != o.size_) return false;

    for (size_t i=0; i<size_; ++i) {
        if (data_[i] != o.data_[i]) return false;
    }
    return true;
  }

  std::strong_ordering operator<=>(const my_vec& o) const {
    if (size_ != o.size_) return size_ <=> o.size_;

    for (size_t i=0; i<o.size_; ++i) {
        if (auto cmp = data_[i] <=> o.data_[i]; cmp != 0) 
            return cmp;
    }
    return std::strong_ordering::equal;
  }

  size_t size() const noexcept { return size_; }
  size_t cap() const noexcept { return cap_; }
  bool empty() const noexcept { return size_ == 0; }

  // 迭代器
  using iterator = T*;
  iterator begin() const { return data_; }
  iterator end() const { return data_ + size_; }
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
      std::cout << *it;
  }
  std::cout << std::endl;
  return 0;
}

