// Single-producer / single-consumer lock-free ring buffer.
//
// Used to hand blocks of samples from the audio thread (producer) to the UI (consumer) for
// spectra, waveforms and scopes. The audio thread never waits and never allocates: if the UI is
// slow and the ring is full, push() drops the newest samples and returns how many it wrote.
// Capacity is fixed at construction (allocation happens there, on the message thread).
#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <vector>

namespace pat {

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4324)  // padding from alignas is intended
#endif
template <class T>
class SpscFifo {
public:
    explicit SpscFifo(std::size_t capacity) : buf_(roundUp(capacity)), mask_(buf_.size() - 1) {}

    SpscFifo(const SpscFifo&) = delete;
    SpscFifo& operator=(const SpscFifo&) = delete;

    std::size_t capacity() const noexcept { return buf_.size(); }

    // Producer side (audio thread). Writes as many items as fit; returns the number written.
    std::size_t push(const T* data, std::size_t count) noexcept {
        const std::size_t w = write_.load(std::memory_order_relaxed);
        const std::size_t r = read_.load(std::memory_order_acquire);
        const std::size_t free = buf_.size() - (w - r);
        const std::size_t n = std::min(count, free);
        for (std::size_t i = 0; i < n; ++i) buf_[(w + i) & mask_] = data[i];
        write_.store(w + n, std::memory_order_release);
        return n;
    }

    // Consumer side (UI thread). Reads up to `count` items; returns the number read.
    std::size_t pop(T* out, std::size_t count) noexcept {
        const std::size_t r = read_.load(std::memory_order_relaxed);
        const std::size_t w = write_.load(std::memory_order_acquire);
        const std::size_t n = std::min(count, w - r);
        for (std::size_t i = 0; i < n; ++i) out[i] = buf_[(r + i) & mask_];
        read_.store(r + n, std::memory_order_release);
        return n;
    }

    // Consumer side: number of items waiting.
    std::size_t available() const noexcept {
        return write_.load(std::memory_order_acquire) - read_.load(std::memory_order_relaxed);
    }

    // Consumer side: throws away everything but the newest `keep` items (the UI only ever wants
    // the most recent audio, never a backlog).
    void skipToNewest(std::size_t keep) noexcept {
        const std::size_t w = write_.load(std::memory_order_acquire);
        const std::size_t r = read_.load(std::memory_order_relaxed);
        if (w - r > keep) read_.store(w - keep, std::memory_order_release);
    }

private:
    static std::size_t roundUp(std::size_t n) {
        std::size_t p = 2;
        while (p < n) p <<= 1;
        return p;
    }

    std::vector<T> buf_;
    std::size_t mask_;
    // Monotonic counters (wrap-around is harmless with unsigned arithmetic). Each sits on its
    // own cache line so the audio and UI threads do not slow each other down (padding intended).
    alignas(64) std::atomic<std::size_t> write_{0};
    alignas(64) std::atomic<std::size_t> read_{0};
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

}  // namespace pat
