#pragma once

#include <atomic>
#include <cstddef>
#include <new>

namespace telemetry {

// SPSC ring; Cap-1 usable

template <typename T, std::size_t Cap>
class RingBuffer {
  static_assert(Cap >= 2, "Ring capacity must be at least 2");

 public:
  RingBuffer() = default;

  RingBuffer(const RingBuffer&) = delete;
  RingBuffer& operator=(const RingBuffer&) = delete;

  bool enqueue(const T& data) {
    const std::size_t current_head = head_.load(std::memory_order_relaxed);
    const std::size_t next_head = (current_head + 1) % Cap;
    const std::size_t current_tail = tail_.load(std::memory_order_acquire);
    if (next_head == current_tail) {
      return false;
    }
    buffer_[current_head] = data;
    head_.store(next_head, std::memory_order_release);
    return true;
  }

  bool dequeue(T& out) {
    const std::size_t current_tail = tail_.load(std::memory_order_relaxed);
    const std::size_t current_head = head_.load(std::memory_order_acquire);
    if (current_tail == current_head) {
      return false;
    }
    out = buffer_[current_tail];
    const std::size_t next_tail = (current_tail + 1) % Cap;
    tail_.store(next_tail, std::memory_order_release);
    return true;
  }

  std::size_t size_approx() const {
    const std::size_t h = head_.load(std::memory_order_acquire);
    const std::size_t t = tail_.load(std::memory_order_acquire);
    return (h >= t) ? (h - t) : (Cap - (t - h));
  }

 private:
  alignas(64) std::atomic<std::size_t> head_{0};
  alignas(64) std::atomic<std::size_t> tail_{0};
  alignas(64) T buffer_[Cap]{};
};

}  // namespace telemetry
