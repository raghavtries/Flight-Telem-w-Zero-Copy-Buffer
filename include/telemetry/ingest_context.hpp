#pragma once

#include "telemetry/buffer_pool.hpp"
#include "telemetry/config.hpp"
#include "telemetry/metrics.hpp"
#include "telemetry/ring_buffer.hpp"
#include "telemetry/wal.hpp"

#include <atomic>
#include <cstdint>

namespace telemetry {

// shared recv/writer; handler closes sock

struct IngestContext {
  std::atomic<bool> running{true};
  PacketBufferPool packet_pool;
  DescriptorPool desc_pool;
  RingBuffer<PacketDescriptor*, RING_BUFFER_SIZE> main_ring;
  Metrics metrics;
  LatencyHistogram latency;
  std::atomic<std::uint64_t> next_sequence{1};
};

void run_udp_receiver(int sockfd, IngestContext& ctx);
void run_disk_writer(IngestContext& ctx, WalWriter& wal);

}  // namespace telemetry
