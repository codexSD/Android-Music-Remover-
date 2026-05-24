#include "RingBuffer.h"

#include <atomic>
#include <thread>
#include <vector>

#include "TinyTest.h"

using vocalremover::SpscRingBuffer;

TEST(RingBuffer, StartsEmpty) {
    SpscRingBuffer<float> rb(8);
    CHECK_EQ(rb.capacity(), static_cast<size_t>(8));
    CHECK_EQ(rb.readAvailable(), static_cast<size_t>(0));
    CHECK_EQ(rb.writeAvailable(), static_cast<size_t>(8));
}

TEST(RingBuffer, WriteThenRead) {
    SpscRingBuffer<float> rb(8);
    float in[4] = {1.f, 2.f, 3.f, 4.f};
    CHECK_EQ(rb.write(in, 4), static_cast<size_t>(4));
    CHECK_EQ(rb.readAvailable(), static_cast<size_t>(4));
    CHECK_EQ(rb.writeAvailable(), static_cast<size_t>(4));

    float out[4] = {0};
    CHECK_EQ(rb.read(out, 4), static_cast<size_t>(4));
    for (int i = 0; i < 4; ++i) CHECK_EQ(out[i], in[i]);
    CHECK_EQ(rb.readAvailable(), static_cast<size_t>(0));
}

TEST(RingBuffer, WriteSaturatesAtCapacity) {
    SpscRingBuffer<int> rb(4);
    int in[6] = {1, 2, 3, 4, 5, 6};
    // Only 4 slots free; write must report a short count and never overflow.
    CHECK_EQ(rb.write(in, 6), static_cast<size_t>(4));
    CHECK_EQ(rb.writeAvailable(), static_cast<size_t>(0));
    CHECK_EQ(rb.write(in, 1), static_cast<size_t>(0));
}

TEST(RingBuffer, ReadUnderrunsGracefully) {
    SpscRingBuffer<int> rb(4);
    int in[2] = {7, 8};
    rb.write(in, 2);
    int out[4] = {0, 0, 0, 0};
    CHECK_EQ(rb.read(out, 4), static_cast<size_t>(2));
    CHECK_EQ(out[0], 7);
    CHECK_EQ(out[1], 8);
}

TEST(RingBuffer, WrapAroundPreservesOrder) {
    SpscRingBuffer<int> rb(4);
    // Advance the cursors so the next writes straddle the wrap point.
    int seed[3] = {10, 11, 12};
    rb.write(seed, 3);
    int drain[3] = {0, 0, 0};
    rb.read(drain, 3);  // readPos = writePos = 3, buffer logically empty

    int in[4] = {20, 21, 22, 23};
    CHECK_EQ(rb.write(in, 4), static_cast<size_t>(4));  // wraps across index 0
    int out[4] = {0, 0, 0, 0};
    CHECK_EQ(rb.read(out, 4), static_cast<size_t>(4));
    for (int i = 0; i < 4; ++i) CHECK_EQ(out[i], in[i]);
}

TEST(RingBuffer, ClearResetsState) {
    SpscRingBuffer<int> rb(4);
    int in[3] = {1, 2, 3};
    rb.write(in, 3);
    rb.clear();
    CHECK_EQ(rb.readAvailable(), static_cast<size_t>(0));
    CHECK_EQ(rb.writeAvailable(), static_cast<size_t>(4));
}

// The real reason this buffer exists: one producer thread and one consumer
// thread hammering it with no lock. Run this under ThreadSanitizer to catch
// data races, and verify the consumer observes every value exactly once and
// in order.
TEST(RingBuffer, ConcurrentProducerConsumerPreservesEverySample) {
    constexpr size_t kCapacity = 1024;          // power of two
    constexpr uint64_t kTotal = 2'000'000;      // items pushed through
    SpscRingBuffer<uint64_t> rb(kCapacity);

    std::atomic<bool> producerFailed{false};
    std::atomic<uint64_t> nextExpected{0};
    std::atomic<bool> orderFailed{false};

    std::thread producer([&] {
        uint64_t sent = 0;
        while (sent < kTotal) {
            const uint64_t batch = (sent + 64 <= kTotal) ? 64 : (kTotal - sent);
            uint64_t tmp[64];
            for (uint64_t i = 0; i < batch; ++i) tmp[i] = sent + i;
            size_t off = 0;
            while (off < batch) {
                const size_t n = rb.write(tmp + off, batch - off);
                off += n;
                if (n == 0) std::this_thread::yield();
            }
            sent += batch;
        }
    });

    std::thread consumer([&] {
        uint64_t received = 0;
        uint64_t expected = 0;
        uint64_t tmp[128];
        while (received < kTotal) {
            const size_t n = rb.read(tmp, 128);
            if (n == 0) {
                std::this_thread::yield();
                continue;
            }
            for (size_t i = 0; i < n; ++i) {
                if (tmp[i] != expected) orderFailed.store(true);
                ++expected;
            }
            received += n;
        }
        nextExpected.store(expected);
    });

    producer.join();
    consumer.join();

    CHECK(!producerFailed.load());
    CHECK(!orderFailed.load());
    CHECK_EQ(nextExpected.load(), kTotal);
}
