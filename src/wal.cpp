#include "telemetry/wal.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <vector>

#if (defined(__x86_64__) || defined(__i386__)) && defined(__SSE4_2__)
#include <nmmintrin.h>
#endif
#if defined(__aarch64__) && defined(__ARM_FEATURE_CRC32)
#include <arm_acle.h>
#endif

namespace telemetry {

namespace {

std::uint64_t steady_now_ns() {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
}

#if !((defined(__x86_64__) || defined(__i386__)) && defined(__SSE4_2__)) && \
    !(defined(__aarch64__) && defined(__ARM_FEATURE_CRC32))
std::uint32_t crc32c_sw(std::uint32_t crc, const std::uint8_t* data, std::size_t len) {
  crc = ~crc;
  for (std::size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int b = 0; b < 8; ++b) {
      crc = (crc >> 1) ^ (0x82F63B78u & (0u - (crc & 1u)));
    }
  }
  return ~crc;
}
#endif

}  // namespace

std::uint32_t crc32c_compute(const void* data, std::size_t len) {
  const auto* p = static_cast<const std::uint8_t*>(data);
#if (defined(__x86_64__) || defined(__i386__)) && defined(__SSE4_2__)
  std::uint32_t crc = 0xFFFFFFFFu;
  std::size_t i = 0;
  for (; (i + 8) <= len; i += 8) {
    std::uint64_t v = 0;
    std::memcpy(&v, p + i, 8);
    crc = static_cast<std::uint32_t>(_mm_crc32_u64(crc, v));
  }
  for (; i < len; ++i) {
    crc = _mm_crc32_u8(crc, p[i]);
  }
  return crc ^ 0xFFFFFFFFu;
#elif defined(__aarch64__) && defined(__ARM_FEATURE_CRC32)
  std::uint32_t crc = 0xFFFFFFFFu;
  std::size_t i = 0;
  for (; (i + 8) <= len; i += 8) {
    std::uint64_t v = 0;
    std::memcpy(&v, p + i, 8);
    crc = __crc32cd(crc, v);
  }
  for (; (i + 4) <= len; i += 4) {
    std::uint32_t v = 0;
    std::memcpy(&v, p + i, 4);
    crc = __crc32cw(crc, v);
  }
  for (; i < len; ++i) {
    crc = __crc32cb(crc, p[i]);
  }
  return crc ^ 0xFFFFFFFFu;
#else
  return crc32c_sw(0, p, len);
#endif
}

std::size_t WalWriter::padded_record_bytes(std::uint32_t payload_len) {
  const std::size_t raw = sizeof(WalRecordHeader) + static_cast<std::size_t>(payload_len);
  return (raw + 7u) & ~std::size_t{7u};
}

WalWriter::WalWriter(std::string path) : path_(std::move(path)) {
  batch_cap_ = DISK_BLOCK_SIZE * 64;
  if (::posix_memalign(reinterpret_cast<void**>(&batch_), DISK_BLOCK_SIZE, batch_cap_) != 0) {
    std::fprintf(stderr, "posix_memalign failed for WAL batch\n");
    std::abort();
  }
  std::memset(batch_, 0, batch_cap_);

#if defined(__linux__)
  fd_ = ::open(path_.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_DIRECT, 0644);
  if (fd_ >= 0) {
    use_odirect_ = true;
  } else {
    fd_ = ::open(path_.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_ < 0) {
      std::perror("open WAL");
      std::abort();
    }
    std::fprintf(stderr, "WAL: O_DIRECT open failed; retry without O_DIRECT (errno was reported).\n");
  }
#else
  fd_ = ::open(path_.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd_ < 0) {
    std::perror("open WAL");
    std::abort();
  }
#endif
  file_offset_ = 0;
  last_sync_ns_ = steady_now_ns();
}

WalWriter::~WalWriter() {
  if (batch_ != nullptr) {
    std::free(batch_);
  }
  if (fd_ >= 0) {
    ::close(fd_);
  }
}

void WalWriter::flush_batch(Metrics& metrics, bool force_sync) {
  if (batch_used_ == 0) {
    return;
  }

  std::size_t to_write = batch_used_;
#if defined(__linux__)
  if (use_odirect_) {
    to_write = (to_write + DISK_BLOCK_SIZE - 1) / DISK_BLOCK_SIZE * DISK_BLOCK_SIZE;
    if (to_write > batch_cap_) {
      to_write = batch_cap_;
    }
    std::memset(batch_ + batch_used_, 0, to_write - batch_used_);
  }
#endif

  const ssize_t n =
      ::pwrite(fd_, batch_, to_write, static_cast<off_t>(file_offset_));
  if (n < 0 || static_cast<std::size_t>(n) != to_write) {
    std::perror("pwrite WAL");
    std::abort();
  }
  metrics.bytes_written.fetch_add(static_cast<std::uint64_t>(n), std::memory_order_relaxed);
  file_offset_ += static_cast<std::uint64_t>(n);
  batch_used_ = 0;

  const std::uint64_t now = steady_now_ns();
  const bool time_due = (now - last_sync_ns_) >= GROUP_COMMIT_NS;
  if (force_sync || time_due) {
    const auto t0 = steady_now_ns();
#if defined(__APPLE__)
    if (::fsync(fd_) != 0) {
#else
    if (::fdatasync(fd_) != 0) {
#endif
      std::perror("fdatasync/fsync WAL");
      std::abort();
    }
    const auto t1 = steady_now_ns();
    metrics.fdatasync_count.fetch_add(1, std::memory_order_relaxed);
    metrics.fdatasync_total_ns.fetch_add(t1 - t0, std::memory_order_relaxed);
    last_sync_ns_ = t1;
  }
}

void WalWriter::append_record(const PacketDescriptor& d, bool payload_valid, std::uint64_t sequence,
                              Metrics& metrics, LatencyHistogram* lat) {
  const std::uint32_t plen = d.len;
  const std::size_t rec_bytes = padded_record_bytes(plen);
  if (rec_bytes > batch_cap_) {
    std::fprintf(stderr, "WAL record larger than batch buffer\n");
    std::abort();
  }
  if (batch_used_ + rec_bytes > batch_cap_) {
    flush_batch(metrics, false);
  }

  WalRecordHeader hdr{};
  hdr.magic = WAL_MAGIC;
  hdr.version = WAL_VERSION;
  hdr.header_len = static_cast<std::uint16_t>(sizeof(WalRecordHeader));
  hdr.payload_len = plen;
  hdr.timestamp_ns = d.timestamp_ns;
  hdr.sequence_no = sequence;
  hdr.flags = payload_valid ? 0u : WAL_FLAG_PAYLOAD_INVALID;
  hdr.reserved0 = 0;
  hdr.crc32c = crc32c_compute(d.data, plen);

  unsigned char* dst = batch_ + batch_used_;
  std::memcpy(dst, &hdr, sizeof(hdr));
  std::memcpy(dst + sizeof(hdr), d.data, plen);
  if (rec_bytes > sizeof(hdr) + plen) {
    std::memset(dst + sizeof(hdr) + plen, 0, rec_bytes - sizeof(hdr) - plen);
  }
  batch_used_ += rec_bytes;

  metrics.wal_records_written.fetch_add(1, std::memory_order_relaxed);
  if (lat != nullptr) {
    const std::uint64_t now = steady_now_ns();
    if (now > d.timestamp_ns) {
      lat->record(now - d.timestamp_ns);
    }
  }

  if (batch_used_ >= WAL_BATCH_FLUSH_BYTES) {
    flush_batch(metrics, false);
  }
}

void WalWriter::flush_all(Metrics& metrics) {
  flush_batch(metrics, true);
}

std::uint64_t wal_recover_verify(const char* path) {
  const int fd = ::open(path, O_RDONLY);
  if (fd < 0) {
    std::perror("recover open");
    return 0;
  }
  std::uint64_t good = 0;
  std::uint64_t offset = 0;
  for (;;) {
    WalRecordHeader hdr{};
    ssize_t rn = ::pread(fd, &hdr, sizeof(hdr), static_cast<off_t>(offset));
    if (rn == 0) {
      break;
    }
    if (rn < static_cast<ssize_t>(sizeof(hdr))) {
      std::fprintf(stderr, "recover: truncated header at offset %llu\n",
                   static_cast<unsigned long long>(offset));
      break;
    }
    if (hdr.magic != WAL_MAGIC) {
      std::fprintf(stderr, "recover: bad magic at offset %llu\n", static_cast<unsigned long long>(offset));
      break;
    }
    if (hdr.version != WAL_VERSION || hdr.header_len != sizeof(WalRecordHeader)) {
      std::fprintf(stderr, "recover: unsupported header layout at %llu\n",
                   static_cast<unsigned long long>(offset));
      break;
    }
    if (hdr.payload_len > MAX_PACKET_SIZE) {
      std::fprintf(stderr, "recover: absurd payload_len at %llu\n", static_cast<unsigned long long>(offset));
      break;
    }
    const std::size_t rec = WalWriter::padded_record_bytes(hdr.payload_len);
    std::vector<unsigned char> payload(hdr.payload_len);
    if (hdr.payload_len > 0) {
      const ssize_t pr = ::pread(fd, payload.data(), hdr.payload_len,
                                 static_cast<off_t>(offset + sizeof(WalRecordHeader)));
      if (pr < static_cast<ssize_t>(hdr.payload_len)) {
        std::fprintf(stderr, "recover: truncated payload at %llu\n", static_cast<unsigned long long>(offset));
        break;
      }
    }
    const std::uint32_t expect = crc32c_compute(payload.data(), hdr.payload_len);
    if (expect != hdr.crc32c) {
      std::fprintf(stderr, "recover: CRC mismatch seq=%llu at offset %llu\n",
                   static_cast<unsigned long long>(hdr.sequence_no),
                   static_cast<unsigned long long>(offset));
      break;
    }
    ++good;
    offset += rec;
  }
  ::close(fd);
  std::fprintf(stderr, "recover: verified_records=%llu\n", static_cast<unsigned long long>(good));
  return good;
}

}  // namespace telemetry
