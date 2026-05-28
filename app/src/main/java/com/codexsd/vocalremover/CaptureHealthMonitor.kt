package com.codexsd.vocalremover

/**
 * Health of an active capture session, derived from the capture RMS over time.
 *
 * Some apps opt out of AudioPlaybackCapture (FLAG_SECURE / ALLOW_CAPTURE_BY_NONE),
 * in which case the system hands us continuous silence even though media is
 * playing. We can't read another app's capture policy, but we can notice that
 * we've been recording nothing but silence and tell the user.
 */
sealed interface CaptureHealth {
    /** Too early to judge (within the startup grace period). */
    data object Starting : CaptureHealth

    /** Receiving audio (or silence that hasn't lasted long enough to worry). */
    data object Healthy : CaptureHealth

    /** Sustained silence while capturing — the source app likely blocks capture. */
    data object Blocked : CaptureHealth
}

/**
 * Pure state machine: fed (timestamp, rms) samples, it reports [CaptureHealth].
 * No Android dependency, so it is unit tested directly.
 *
 * @param silenceRmsThreshold RMS at/below which a block counts as silent.
 * @param blockedAfterSilentMs how long continuous silence must persist (after
 *   the grace period) before reporting [CaptureHealth.Blocked].
 * @param graceMs initial period after start during which we never judge, to let
 *   the pipeline prime and playback begin.
 */
class CaptureHealthMonitor(
    private val silenceRmsThreshold: Float = 1e-4f,
    private val blockedAfterSilentMs: Long = 2_000,
    private val graceMs: Long = 1_000,
) {
    private var startMs: Long = UNSET
    private var silentSinceMs: Long = UNSET

    fun reset() {
        startMs = UNSET
        silentSinceMs = UNSET
    }

    fun update(nowMs: Long, rms: Float): CaptureHealth {
        if (startMs == UNSET) startMs = nowMs
        if (nowMs - startMs < graceMs) return CaptureHealth.Starting

        if (rms <= silenceRmsThreshold) {
            if (silentSinceMs == UNSET) silentSinceMs = nowMs
            return if (nowMs - silentSinceMs >= blockedAfterSilentMs) {
                CaptureHealth.Blocked
            } else {
                CaptureHealth.Healthy
            }
        }
        silentSinceMs = UNSET
        return CaptureHealth.Healthy
    }

    private companion object {
        const val UNSET = -1L
    }
}
