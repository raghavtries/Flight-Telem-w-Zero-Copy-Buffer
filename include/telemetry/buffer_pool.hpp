#pragma once

#include "telemetry/config.hpp"
#include "telemetry/packet_descriptor.hpp"
#include "telemetry/ring_buffer.hpp"

#include <cstddef>
#include <vector>

namespace telemetry {

// SPSC pools; recv dequeues free, writer enqueues.

class PacketBufferPool {
 public:
  PacketBufferPool();

  char* acquire();

  void release(char* p);

 private:
  std::vector<char> storage_;
  RingBuffer<char*, POOL_RING_CAP> free_;
};

class DescriptorPool {
 public:
  DescriptorPool();

  PacketDescriptor* acquire();
  void release(PacketDescriptor* d);

 private:
  std::vector<PacketDescriptor> storage_;
  RingBuffer<PacketDescriptor*, POOL_RING_CAP> free_;
};

}  // namespace telemetry
