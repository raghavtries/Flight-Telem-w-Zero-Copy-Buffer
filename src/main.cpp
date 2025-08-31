#include "telemetry/ingest_context.hpp"
#include "telemetry/metrics.hpp"
#include "telemetry/wal.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

#include <arpa/inet.h>
#include <csignal>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

volatile sig_atomic_t g_sigstop = 0;
volatile sig_atomic_t g_dump_metrics = 0;
int g_udp_sock = -1;

void on_signal(int sig) {
  if (sig == SIGUSR1) {
    g_dump_metrics = 1;
    return;
  }
  g_sigstop = 1;
  const int s = g_udp_sock;
  if (s >= 0) {
    ::close(s);
  }
  g_udp_sock = -1;
}

int udp_bind(std::uint16_t port) {
  const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    std::perror("socket");
    return -1;
  }
  int one = 1;
  if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) != 0) {
    std::perror("setsockopt SO_REUSEADDR");
    ::close(fd);
    return -1;
  }
  const int rcv = telemetry::SOCKET_RCVBUF_BYTES;
  if (::setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcv, sizeof(rcv)) != 0) {
    std::perror("setsockopt SO_RCVBUF");
    ::close(fd);
    return -1;
  }
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    std::perror("bind");
    ::close(fd);
    return -1;
  }
  return fd;
}

void install_handlers() {
  struct sigaction sa {};
  sa.sa_handler = on_signal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  if (::sigaction(SIGINT, &sa, nullptr) != 0) {
    std::perror("sigaction SIGINT");
    std::abort();
  }
  if (::sigaction(SIGTERM, &sa, nullptr) != 0) {
    std::perror("sigaction SIGTERM");
    std::abort();
  }
  if (::sigaction(SIGUSR1, &sa, nullptr) != 0) {
    std::perror("sigaction SIGUSR1");
    std::abort();
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc >= 3 && std::strcmp(argv[1], "--recover") == 0) {
    telemetry::wal_recover_verify(argv[2]);
    return 0;
  }

  std::string wal_path = "telemetry.wal";
  if (argc >= 2) {
    wal_path = argv[1];
  }

  install_handlers();

  const int sock = udp_bind(telemetry::UDP_PORT);
  if (sock < 0) {
    return 1;
  }
  g_udp_sock = sock;

  telemetry::IngestContext ctx;
  telemetry::WalWriter wal(wal_path);

  std::thread recv_thread([&] { telemetry::run_udp_receiver(sock, ctx); });
  std::thread writer_thread([&] { telemetry::run_disk_writer(ctx, wal); });

  std::fprintf(stderr,
               "telemetry_ingestor: listening UDP port %u, WAL %s — SIGINT stop, SIGUSR1 metrics\n",
               static_cast<unsigned>(telemetry::UDP_PORT),
               wal_path.c_str());

  while (!g_sigstop) { if (g_dump_metrics) {
      g_dump_metrics = 0;
      telemetry::print_metrics_snapshot(ctx.metrics);
      ctx.latency.print_percentiles("wal_e2e");
    } std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  recv_thread.join();

  ctx.running.store(false, std::memory_order_release);
  writer_thread.join();

  wal.flush_all(ctx.metrics);

  telemetry::print_metrics_snapshot(ctx.metrics);
  ctx.latency.print_percentiles("wal_e2e_final");
  std::fprintf(stderr, "shutdown complete\n");
  return 0;
}
