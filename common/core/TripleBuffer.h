// Lock-free triple buffer: one writer thread (the editor) publishes complete values, one reader
// thread (audio) always gets the newest complete value without waiting and without copies.
//
//   writer: T& w = buf.writeSlot(); ...fill w...; buf.publish();
//   reader: const T& r = buf.read();   // newest published value (or the previous one)
//
// T is preallocated three times; nothing is allocated after construction.
#pragma once

#include <array>
#include <atomic>
#include <cstdint>

namespace pat {

template <typename T>
class TripleBuffer {
public:
    TripleBuffer() = default;
    explicit TripleBuffer(const T& initial) { slots_.fill(initial); }

    T& writeSlot() noexcept { return slots_[write_]; }
    void publish() noexcept { write_ = middle_.exchange(write_ | kFresh, std::memory_order_acq_rel) & kIndex; }

    const T& read() noexcept {
        if (middle_.load(std::memory_order_acquire) & kFresh) read_ = middle_.exchange(read_, std::memory_order_acq_rel) & kIndex;
        return slots_[read_];
    }

private:
    static constexpr std::uint32_t kFresh = 4, kIndex = 3;
    std::array<T, 3> slots_{};
    std::uint32_t write_ = 0, read_ = 1;
    std::atomic<std::uint32_t> middle_{2};
};

}  // namespace pat
