#include "telemetry/ingest_context.hpp"

#include "telemetry/config.hpp"

#include <chrono>
#include <cerrno>
#include <cstring>
#include <thread>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace telemetry {

namespace {

std::uint64_t monotonic_now_ns() {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
}

}  // namespace

void run_udp_receiver(int sockfd, IngestContext& ctx) {
  while (ctx.running.load(std::memory_order_acquire)) {
    char* buf = ctx.packet_pool.acquire();
    PacketDescriptor* desc = ctx.desc_pool.acquire();

    sockaddr_storage peer{};
    socklen_t peer_len = sizeof(peer);

    const ssize_t n =
        ::recvfrom(sockfd, buf, MAX_PACKET_SIZE, 0, reinterpret_cast<sockaddr*>(&peer), &peer_len);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      if (errno != EBADF && errno != EINVAL) {
        ctx.metrics.recv_errors.fetch_add(1, std::memory_order_relaxed);
      }
      ctx.packet_pool.release(buf);
      ctx.desc_pool.release(desc);
      if (errno == EBADF || errno == EINVAL) {
        break;
      }
      continue;
    }

    desc->data = buf;
    desc->len = static_cast<std::uint32_t>(n);
    desc->timestamp_ns = monotonic_now_ns();
    desc->sender = peer;
    desc->sender_len = peer_len;

    ctx.metrics.packets_received.fetch_add(1, std::memory_order_relaxed);

    while (!ctx.main_ring.enqueue(desc)) {
      ctx.metrics.ring_full_spins.fetch_add(1, std::memory_order_relaxed);
      std::this_thread::yield();
    }
    ctx.metrics.packets_enqueued.fetch_add(1, std::memory_order_relaxed);
  }
}

}  // namespace telemetry
