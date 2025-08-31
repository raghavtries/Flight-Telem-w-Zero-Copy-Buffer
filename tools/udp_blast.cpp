#include "telemetry/config.hpp"
#include "telemetry/validate.hpp"

#include <cstdlib>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char** argv) {
  std::uint64_t rate_per_sec = 2000;
  std::uint64_t total = 10'000;
  if (argc >= 2) {
    rate_per_sec = static_cast<std::uint64_t>(std::atoll(argv[1]));
  }
  if (argc >= 3) {
    total = static_cast<std::uint64_t>(std::atoll(argv[2]));
  }

  const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    std::perror("socket");
    return 1;
  }

  sockaddr_in dst{};
  dst.sin_family = AF_INET;
  dst.sin_port = htons(telemetry::UDP_PORT);
  dst.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  telemetry::TelemetryPayloadWire hdr{};
  hdr.magic = telemetry::TELEMETRY_PAYLOAD_MAGIC;
  hdr.protocol_ver = telemetry::TELEMETRY_PROTOCOL_VERSION;
  hdr.reserved0 = 0;
  hdr.aircraft_id = 42;
  hdr.timestamp_us = 0;

  std::uint8_t buf[telemetry::MAX_PACKET_SIZE]{};
  const std::size_t payload_len = telemetry::TELEMETRY_MIN_PAYLOAD;
  std::memcpy(buf, &hdr, sizeof(hdr));
  std::memset(buf + sizeof(hdr), 0, payload_len - sizeof(hdr));

  const auto interval =
      std::chrono::duration<double>(1.0 / static_cast<double>(rate_per_sec));
  auto next = std::chrono::steady_clock::now();

  for (std::uint64_t i = 0; i < total; ++i) {
    hdr.timestamp_us = i;
    std::memcpy(buf, &hdr, sizeof(hdr));
    const ssize_t n =
        ::sendto(fd, buf, payload_len, 0, reinterpret_cast<sockaddr*>(&dst), sizeof(dst));
    if (n < 0) {
      std::perror("sendto");
      break;
    }
    next += std::chrono::duration_cast<std::chrono::steady_clock::duration>(interval);
    std::this_thread::sleep_until(next);
  }

  ::close(fd);
  std::fprintf(stderr, "udp_blast: sent %llu datagrams\n", static_cast<unsigned long long>(total));
  return 0;
}
