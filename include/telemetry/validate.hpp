#pragma once

#include "telemetry/packet_descriptor.hpp"

#include <cstdint>

namespace telemetry {

#pragma pack(push, 1)
struct TelemetryPayloadWire {
  std::uint32_t magic;
  std::uint16_t protocol_ver;
  std::uint16_t reserved0;
  std::uint64_t aircraft_id;
  std::uint64_t timestamp_us;
};
#pragma pack(pop)

inline constexpr std::uint32_t TELEMETRY_PAYLOAD_MAGIC = 0x4C564654;

inline constexpr std::uint16_t TELEMETRY_PROTOCOL_VERSION = 1;

struct ValidationResult {
  bool ok{false};
};

// in-place read of d.data

ValidationResult validate_payload(const PacketDescriptor& d);

}  // namespace telemetry
