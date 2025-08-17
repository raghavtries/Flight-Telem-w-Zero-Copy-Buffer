#include "telemetry/validate.hpp"

#include "telemetry/config.hpp"

#include <cstring>

namespace telemetry {

ValidationResult validate_payload(const PacketDescriptor& d) {
  ValidationResult r;
  if (d.data == nullptr) {
    return r;
  }
  if (d.len < TELEMETRY_MIN_PAYLOAD || d.len > MAX_PACKET_SIZE) {
    return r;
  }
  if (d.len < sizeof(TelemetryPayloadWire)) {
    return r;
  }
  TelemetryPayloadWire hdr{};
  std::memcpy(&hdr, d.data, sizeof(hdr));
  if (hdr.magic != TELEMETRY_PAYLOAD_MAGIC) {
    return r;
  }
  if (hdr.protocol_ver != TELEMETRY_PROTOCOL_VERSION) {
    return r;
  }
  if (hdr.aircraft_id == 0) {
    return r;
  }
  constexpr std::uint64_t kMaxTsUs = 48ULL * 3600ULL * 1'000'000ULL;
  if (hdr.timestamp_us > kMaxTsUs) { return r; }
  r.ok = true; return r;
}

}  // namespace telemetry
