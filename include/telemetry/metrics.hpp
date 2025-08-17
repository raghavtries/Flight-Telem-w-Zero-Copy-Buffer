#pragma once

#include "telemetry/config.hpp"

#include <atomic>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace telemetry {

// atomics; alignas reduces false share

struct alignas(64) Metrics {
  std::atomic<std::uint64_t> packets_received{0};
  std::atomic<std::uint64_t> packets_enqueued{0};
  std::atomic<std::uint64_t> ring_full_spins{0};
  std::atomic<std::uint64_t> recv_errors{0};
  std::atomic<std::uint64_t> validation_failures{0};
  std::atomic<std::uint64_t> wal_records_written{0};
  std::atomic<std::uint64_t> bytes_written{0};
  std::atomic<std::uint64_t> fdatasync_count{0};
  std::atomic<std::uint64_t> fdatasync_total_ns{0};
  std::atomic<std::uint64_t> pool_exhausted_spins{0};
};

class LatencyHistogram {
 public:
  void record(std::uint64_t latency_ns) {
    const std::size_t i = index_.fetch_add(1, std::memory_order_relaxed) % LATENCY_SAMPLE_CAP;
    samples_[i] = latency_ns;
    if (count_.load(std::memory_order_relaxed) < LATENCY_SAMPLE_CAP) {
      count_.fetch_add(1, std::memory_order_relaxed);
    }
  }

  void print_percentiles(const char* label) const {
    const std::size_t n = std::min<std::size_t>(count_.load(std::memory_order_relaxed), LATENCY_SAMPLE_CAP);
    if (n == 0) {
      std::fprintf(stderr, "[%s] latency: no samples\n", label);
      return;
    }
    std::vector<std::uint64_t> v(samples_.begin(), samples_.begin() + static_cast<std::ptrdiff_t>(n));
    std::sort(v.begin(), v.end());
    auto pct = [&](double p) -> std::uint64_t {
      const std::size_t idx = static_cast<std::size_t>(p * (n - 1));
      return v[idx];
    };
    std::fprintf(stderr,
                 "[%s] latency ns: n=%zu p50=%llu p95=%llu p99=%llu max=%llu\n",
                 label,
                 n,
                 static_cast<unsigned long long>(pct(0.50)),
                 static_cast<unsigned long long>(pct(0.95)),
                 static_cast<unsigned long long>(pct(0.99)),
                 static_cast<unsigned long long>(v.back()));
  }

 private:
  alignas(64) std::array<std::uint64_t, LATENCY_SAMPLE_CAP> samples_{};
  alignas(64) std::atomic<std::size_t> index_{0};
  alignas(64) std::atomic<std::size_t> count_{0};
};

inline void print_metrics_snapshot(const Metrics& m) {
  std::fprintf(stderr,
               "metrics: recv=%llu enqueued=%llu ring_spins=%llu recv_err=%llu val_fail=%llu "
               "wal=%llu bytes=%llu fdatasync=%llu fdatasync_avg_us=%llu pool_spins=%llu\n",
               static_cast<unsigned long long>(m.packets_received.load(std::memory_order_relaxed)),
               static_cast<unsigned long long>(m.packets_enqueued.load(std::memory_order_relaxed)),
               static_cast<unsigned long long>(m.ring_full_spins.load(std::memory_order_relaxed)),
               static_cast<unsigned long long>(m.recv_errors.load(std::memory_order_relaxed)),
               static_cast<unsigned long long>(m.validation_failures.load(std::memory_order_relaxed)),
               static_cast<unsigned long long>(m.wal_records_written.load(std::memory_order_relaxed)),
               static_cast<unsigned long long>(m.bytes_written.load(std::memory_order_relaxed)),
               static_cast<unsigned long long>(m.fdatasync_count.load(std::memory_order_relaxed)),
               static_cast<unsigned long long>(
                   m.fdatasync_count.load(std::memory_order_relaxed) == 0
                       ? 0
                       : m.fdatasync_total_ns.load(std::memory_order_relaxed) /
                             (1000 * m.fdatasync_count.load(std::memory_order_relaxed))),
               static_cast<unsigned long long>(m.pool_exhausted_spins.load(std::memory_order_relaxed)));
}

}  // namespace telemetry
