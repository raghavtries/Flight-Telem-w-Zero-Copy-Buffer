#include "telemetry/buffer_pool.hpp"

#include <cstdlib>
#include <new>

namespace telemetry {

namespace {

inline constexpr std::size_t kPacketStride = ((MAX_PACKET_SIZE + 63) / 64) * 64;

}  // namespace

PacketBufferPool::PacketBufferPool()
    : storage_(PACKET_POOL_SIZE * kPacketStride) {
  for (std::size_t i = 0; i < PACKET_POOL_SIZE; ++i) {
    char* p = storage_.data() + i * kPacketStride;
    const bool ok = free_.enqueue(p);
    static_cast<void>(ok);
  }
}

char* PacketBufferPool::acquire() {
  char* out = nullptr;
  while (!free_.dequeue(out)) {
  } return out;
}

void PacketBufferPool::release(char* p) {
  const bool ok = free_.enqueue(p);
  static_cast<void>(ok);
}

DescriptorPool::DescriptorPool() : storage_(PACKET_POOL_SIZE) {
  for (std::size_t i = 0; i < PACKET_POOL_SIZE; ++i) {
    const bool ok = free_.enqueue(&storage_[i]);
    static_cast<void>(ok);
  }
}

PacketDescriptor* DescriptorPool::acquire() {
  PacketDescriptor* out = nullptr;
  while (!free_.dequeue(out)) {
  }
  return out;
}

void DescriptorPool::release(PacketDescriptor* d) {
  const bool ok = free_.enqueue(d);
  static_cast<void>(ok);
}

}  // namespace telemetry
