package com.codexsd.vocalremover

import com.google.common.truth.Truth.assertThat
import org.junit.Test

class CaptureHealthMonitorTest {

    private fun monitor() = CaptureHealthMonitor(
        silenceRmsThreshold = 1e-4f,
        blockedAfterSilentMs = 2_000,
        graceMs = 1_000,
    )

    @Test
    fun startsInGracePeriod() {
        val m = monitor()
        assertThat(m.update(0, 0f)).isEqualTo(CaptureHealth.Starting)
        assertThat(m.update(999, 0f)).isEqualTo(CaptureHealth.Starting)
    }

    @Test
    fun audioPresentIsHealthy() {
        val m = monitor()
        m.update(0, 0.2f)
        assertThat(m.update(1_500, 0.2f)).isEqualTo(CaptureHealth.Healthy)
    }

    @Test
    fun sustainedSilenceAfterGraceIsBlocked() {
        val m = monitor()
        m.update(0, 0f)                 // grace
        m.update(1_001, 0f)             // silence begins counting here
        assertThat(m.update(2_500, 0f)).isEqualTo(CaptureHealth.Healthy)  // <2s silent
        assertThat(m.update(3_001, 0f)).isEqualTo(CaptureHealth.Blocked)  // >=2s silent
    }

    @Test
    fun audioResettingSilenceClearsBlockedCountdown() {
        val m = monitor()
        m.update(0, 0f)
        m.update(1_001, 0f)
        m.update(2_500, 0.1f)           // audio arrives, resets the silence timer
        assertThat(m.update(3_600, 0f)).isEqualTo(CaptureHealth.Healthy)  // only 1.1s silent again
    }

    @Test
    fun resetReturnsToGrace() {
        val m = monitor()
        m.update(0, 0f)
        m.update(5_000, 0f)             // would be Blocked
        m.reset()
        assertThat(m.update(10_000, 0f)).isEqualTo(CaptureHealth.Starting)
    }

    @Test
    fun rmsAtThresholdCountsAsSilent() {
        val m = monitor()
        m.update(0, 0f)
        m.update(1_001, 1e-4f)
        assertThat(m.update(3_100, 1e-4f)).isEqualTo(CaptureHealth.Blocked)
    }
}
