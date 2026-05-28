#ifndef VOCALREMOVER_RINGBUFFER_H
#define VOCALREMOVER_RINGBUFFER_H

#include <atomic>
#include <cstddef>
#include <cstring>
#include <memory>

namespace vocalremover {

// Lock-free single-producer / single-consumer ring buffer.
//
// Exactly one thread may call write()/writeAvailable() (the producer) and
// exactly one *other* thread may call read()/readAvailable() (the consumer).
// Any other sharing is undefined. This is the data structure that sits between
// the capture thread and the audio output callback, so it must never block,
// allocate, or take a lock on the hot path.
//
// Capacity must be a power of two. Position counters are monotonically
// increasing and wrap naturally on size_t overflow; the difference of two
// counters is always the correct fill level as long as it never exceeds
// capacity (which the API guarantees).
template <typename T>
class SpscRingBuffer {
public:
    explicit SpscRingBuffer(size_t capacityPow2)
        : capacity_(capacityPow2),
          mask_(capacityPow2 - 1),
          buffer_(new T[capacityPow2]) {
        // capacityPow2 must be a non-zero power of two.
    }

    static bool isPowerOfTwo(size_t v) { return v != 0 && (v & (v - 1)) == 0; }

    size_t capacity() const { return capacity_; }

    // Number of items available to read. Safe to call from the consumer thread.
    size_t readAvailable() const {
        const size_t w = writePos_.load(std::memory_order_acquire);
        const size_t r = readPos_.load(std::memory_order_relaxed);
        return w - r;
    }

    // Free space available to write. Safe to call from the producer thread.
    size_t writeAvailable() const {
        const size_t r = readPos_.load(std::memory_order_acquire);
        const size_t w = writePos_.load(std::memory_order_relaxed);
        return capacity_ - (w - r);
    }

    // Producer-only. Copies up to `count` items in, returns the number written.
    size_t write(const T* src, size_t count) {
        const size_t r = readPos_.load(std::memory_order_acquire);
        const size_t w = writePos_.load(std::memory_order_relaxed);
        const size_t free = capacity_ - (w - r);
        const size_t n = count < free ? count : free;
        if (n == 0) return 0;

        const size_t start = w & mask_;
        const size_t firstChunk = (capacity_ - start) < n ? (capacity_ - start) : n;
        std::memcpy(buffer_.get() + start, src, firstChunk * sizeof(T));
        if (n > firstChunk) {
            std::memcpy(buffer_.get(), src + firstChunk, (n - firstChunk) * sizeof(T));
        }

        writePos_.store(w + n, std::memory_order_release);
        return n;
    }

    // Consumer-only. Copies up to `count` items out, returns the number read.
    size_t read(T* dst, size_t count) {
        const size_t w = writePos_.load(std::memory_order_acquire);
        const size_t r = readPos_.load(std::memory_order_relaxed);
        const size_t avail = w - r;
        const size_t n = count < avail ? count : avail;
        if (n == 0) return 0;

        const size_t start = r & mask_;
        const size_t firstChunk = (capacity_ - start) < n ? (capacity_ - start) : n;
        std::memcpy(dst, buffer_.get() + start, firstChunk * sizeof(T));
        if (n > firstChunk) {
            std::memcpy(dst + firstChunk, buffer_.get(), (n - firstChunk) * sizeof(T));
        }

        readPos_.store(r + n, std::memory_order_release);
        return n;
    }

    // Not thread-safe. Only call when neither producer nor consumer is running.
    void clear() {
        writePos_.store(0, std::memory_order_relaxed);
        readPos_.store(0, std::memory_order_relaxed);
    }

private:
    const size_t capacity_;
    const size_t mask_;
    std::unique_ptr<T[]> buffer_;

    // Keep producer and consumer cursors on separate cache lines to avoid
    // false sharing between the two threads.
    alignas(64) std::atomic<size_t> writePos_{0};
    alignas(64) std::atomic<size_t> readPos_{0};
};

}  // namespace vocalremover

#endif  // VOCALREMOVER_RINGBUFFER_H
