#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace hft {

// Single-producer/single-consumer ring buffer.
template <typename T, size_t Capacity>
class SPSCRing {
public:
    SPSCRing() : head_(0), tail_(0) {
        static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of two");
    }

    bool push(const T& v) {
        const auto head = head_.load(std::memory_order_relaxed);
        const auto next = (head + 1) & mask();
        if (next == tail_.load(std::memory_order_acquire)) {
            return false;  // full
        }
        data_[head] = v;
        head_.store(next, std::memory_order_release);
        return true;
    }

    std::optional<T> pop() {
        const auto tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return std::nullopt;
        }
        auto v = data_[tail];
        tail_.store((tail + 1) & mask(), std::memory_order_release);
        return v;
    }

    size_t size() const {
        auto head = head_.load(std::memory_order_acquire);
        auto tail = tail_.load(std::memory_order_acquire);
        if (head >= tail) return head - tail;
        return Capacity - (tail - head);
    }

private:
    constexpr size_t mask() const { return Capacity - 1; }

    alignas(64) std::atomic<size_t> head_;
    alignas(64) std::atomic<size_t> tail_;
    std::array<T, Capacity> data_{};
};

}  // namespace hft
