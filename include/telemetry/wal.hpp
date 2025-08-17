#pragma once

#include "telemetry/config.hpp"
#include "telemetry/metrics.hpp"
#include "telemetry/packet_descriptor.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace telemetry {

inline constexpr std::uint32_t WAL_FLAG_PAYLOAD_INVALID = 1u << 0;

#pragma pack(push, 1)
struct WalRecordHeader {
  std::uint32_t magic;
  std::uint16_t version;
  std::uint16_t header_len;
  std::uint32_t payload_len;
  std::uint32_t crc32c;
  std::uint64_t timestamp_ns;
  std::uint64_t sequence_no;
  std::uint32_t flags;
  std::uint32_t reserved0;
};
#pragma pack(pop)

static_assert(sizeof(WalRecordHeader) == 40, "WalRecordHeader size drift");

// crc32c + batched wal

std::uint32_t crc32c_compute(const void* data, std::size_t len);

class WalWriter {
 public:
  explicit WalWriter(std::string path);

  WalWriter(const WalWriter&) = delete;
  WalWriter& operator=(const WalWriter&) = delete;

  ~WalWriter();

  void append_record(const PacketDescriptor& d, bool payload_valid, std::uint64_t sequence,
                     Metrics& metrics, LatencyHistogram* lat);

  void flush_all(Metrics& metrics);

  const std::string& path() const { return path_; }

  static std::size_t padded_record_bytes(std::uint32_t payload_len);

 private:
  void flush_batch(Metrics& metrics, bool force_sync);

  std::string path_;
  int fd_{-1};
  [[maybe_unused]] bool use_odirect_{false};
  std::uint64_t file_offset_{0};
  std::uint64_t last_sync_ns_{0};

  std::size_t batch_used_{0};
  std::size_t batch_cap_{0};
  unsigned char* batch_{nullptr};
};

std::uint64_t wal_recover_verify(const char* path);

}  // namespace telemetry
