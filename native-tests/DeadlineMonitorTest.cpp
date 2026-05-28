#include "DeadlineMonitor.h"

#include "TinyTest.h"

using vocalremover::DeadlineMonitor;

namespace {
// budget 1,000,000 ns (1 ms), overrun above 0.9 ms, 4 consecutive late blocks.
DeadlineMonitor make() { return DeadlineMonitor(1'000'000, 0.9, 4); }
}  // namespace

TEST(DeadlineMonitor, UnderBudgetNeverFires) {
    auto m = make();
    for (int i = 0; i < 100; ++i) CHECK(!m.record(500'000));  // 0.5 ms each
    CHECK(!m.violated());
}

TEST(DeadlineMonitor, SingleSpikeDoesNotFire) {
    auto m = make();
    CHECK(!m.record(500'000));
    CHECK(!m.record(2'000'000));  // one late block
    CHECK(!m.record(500'000));    // recovers; counter resets
    CHECK(!m.violated());
}

TEST(DeadlineMonitor, SustainedOverrunFiresOnce) {
    auto m = make();
    CHECK(!m.record(2'000'000));  // 1 late
    CHECK(!m.record(2'000'000));  // 2
    CHECK(!m.record(2'000'000));  // 3
    CHECK(m.record(2'000'000));   // 4 -> fires
    CHECK(m.violated());
    // Latched: does not fire again.
    CHECK(!m.record(2'000'000));
}

TEST(DeadlineMonitor, ResetClearsState) {
    auto m = make();
    for (int i = 0; i < 4; ++i) m.record(2'000'000);
    CHECK(m.violated());
    m.reset();
    CHECK(!m.violated());
    CHECK(!m.record(2'000'000));  // counting restarts from zero
}

TEST(DeadlineMonitor, RecoveryBreaksTheStreak) {
    auto m = make();
    m.record(2'000'000);
    m.record(2'000'000);
    m.record(500'000);  // recovery resets streak
    CHECK(!m.record(2'000'000));
    CHECK(!m.record(2'000'000));
    CHECK(!m.violated());  // only 2 consecutive since recovery
}
