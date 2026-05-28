#ifndef VOCALREMOVER_DEADLINEMONITOR_H
#define VOCALREMOVER_DEADLINEMONITOR_H

#include <cstdint>

namespace vocalremover {

// Tracks whether the active engine is keeping up with the real-time budget.
// Pure logic with no clock of its own: the caller times each process() call and
// feeds the elapsed nanoseconds in. record() returns true once, at the moment a
// sustained-overrun condition is newly reached, so the caller can raise a
// one-shot violation signal.
//
// "Sustained" means `window` consecutive blocks each exceeding
// `overrunRatio * budget`. A single spike does not trip it.
class DeadlineMonitor {
public:
    DeadlineMonitor(uint64_t budgetNs, double overrunRatio, int window)
        : overrunNs_(static_cast<uint64_t>(budgetNs * overrunRatio)),
          window_(window < 1 ? 1 : window) {}

    // Returns true exactly once when the consecutive-overrun count first reaches
    // the window. Stays false afterwards until reset() (latched), and resets the
    // counter whenever a block comes in under budget.
    bool record(uint64_t elapsedNs) {
        if (elapsedNs > overrunNs_) {
            ++consecutive_;
        } else {
            consecutive_ = 0;
        }
        if (!latched_ && consecutive_ >= window_) {
            latched_ = true;
            return true;
        }
        return false;
    }

    void reset() {
        consecutive_ = 0;
        latched_ = false;
    }

    bool violated() const { return latched_; }

private:
    const uint64_t overrunNs_;
    const int window_;
    int consecutive_ = 0;
    bool latched_ = false;
};

}  // namespace vocalremover

#endif  // VOCALREMOVER_DEADLINEMONITOR_H
