#pragma once

#include <atomic>
#include <cstddef>

namespace hft {

template <typename T, std::size_t CapacityPow2>
class SpscQueue {
 public:
  static_assert((CapacityPow2 & (CapacityPow2 - 1)) == 0, "Capacity must be power-of-two");

  bool push(const T& item) {
    const std::size_t head = head_.load(std::memory_order_relaxed); // why relax?
    const std::size_t next = (head + 1) & mask_;
    if (next == tail_.load(std::memory_order_acquire)) {
      return false; // full?
    }
    buffer_[head] = item;
    head_.store(next, std::memory_order_release);
    return true;
  }

  bool pop(T& out) {
    const std::size_t tail = tail_.load(std::memory_order_relaxed);
    if (tail == head_.load(std::memory_order_acquire)) {
      return false;
    }
    out = buffer_[tail];
    tail_.store((tail + 1) & mask_, std::memory_order_release);
    return true;
  }

  std::size_t size() const {
    const std::size_t head = head_.load(std::memory_order_acquire);
    const std::size_t tail = tail_.load(std::memory_order_acquire);
    return (head + CapacityPow2 - tail) & mask_;  // full and empty?
  }

 private:
  alignas(64) std::atomic<std::size_t> head_{0};
  alignas(64) std::atomic<std::size_t> tail_{0};
  static constexpr std::size_t mask_ = CapacityPow2 - 1;
  T buffer_[CapacityPow2];
};

}  // namespace hft
