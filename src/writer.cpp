#include "telemetry/ingest_context.hpp"

#include "telemetry/validate.hpp"

#include <thread>

namespace telemetry {

void run_disk_writer(IngestContext& ctx, WalWriter& wal) {
  for (;;) {
    PacketDescriptor* desc = nullptr;
    if (ctx.main_ring.dequeue(desc)) {
      const ValidationResult vr = validate_payload(*desc);
      if (!vr.ok) {
        ctx.metrics.validation_failures.fetch_add(1, std::memory_order_relaxed);
      }
      const std::uint64_t seq = ctx.next_sequence.fetch_add(1, std::memory_order_relaxed);
      wal.append_record(*desc, vr.ok, seq, ctx.metrics, &ctx.latency);

      ctx.packet_pool.release(desc->data);
      desc->data = nullptr;
      desc->len = 0;
      ctx.desc_pool.release(desc);

      continue;
    }

    if (!ctx.running.load(std::memory_order_acquire)) {
      break;
    }
    std::this_thread::yield();
  }

  for (;;) {
    PacketDescriptor* desc = nullptr;
    if (!ctx.main_ring.dequeue(desc)) {
      break;
    }
    const ValidationResult vr = validate_payload(*desc);
    if (!vr.ok) {
      ctx.metrics.validation_failures.fetch_add(1, std::memory_order_relaxed);
    }
    const std::uint64_t seq = ctx.next_sequence.fetch_add(1, std::memory_order_relaxed);
    wal.append_record(*desc, vr.ok, seq, ctx.metrics, &ctx.latency);
    ctx.packet_pool.release(desc->data);
    desc->data = nullptr;
    desc->len = 0;
    ctx.desc_pool.release(desc);
  }

  wal.flush_all(ctx.metrics);
}

}  // namespace telemetry
