#pragma once

#include <cstddef>
#include <cstdint>

namespace telemetry {

// knobs: pool/ring/disk/net

inline constexpr std::size_t MAX_PACKET_SIZE = 512;

inline constexpr std::size_t RING_BUFFER_SIZE = 8192;

inline constexpr std::size_t PACKET_POOL_SIZE = RING_BUFFER_SIZE + 64;

inline constexpr std::size_t POOL_RING_CAP = PACKET_POOL_SIZE + 2;

inline constexpr std::size_t DISK_BLOCK_SIZE = 4096;

inline constexpr std::size_t WAL_BATCH_FLUSH_BYTES = DISK_BLOCK_SIZE * 4;

inline constexpr std::uint64_t GROUP_COMMIT_NS = 50'000'000ULL;

inline constexpr std::uint16_t UDP_PORT = 8080;

inline constexpr int SOCKET_RCVBUF_BYTES = 4 * 1024 * 1024;

inline constexpr std::uint32_t WAL_MAGIC = 0x454C4554;

inline constexpr std::uint16_t WAL_VERSION = 1;

inline constexpr std::size_t TELEMETRY_MIN_PAYLOAD = 32;

inline constexpr std::size_t LATENCY_SAMPLE_CAP = 65536;

}  // namespace telemetry
