#pragma once

#include <sys/socket.h>

#include <cstddef>
#include <cstdint>

namespace telemetry {

// ptr from pool through main ring

struct PacketDescriptor {
  char* data{nullptr};
  std::uint32_t len{0};
  std::uint64_t timestamp_ns{0};
  sockaddr_storage sender{};
  socklen_t sender_len{0};
};

}  // namespace telemetry
