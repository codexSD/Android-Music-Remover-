package com.codexsd.vocalremover

import com.google.common.truth.Truth.assertThat
import org.junit.Test

/**
 * Pure-JVM tests for the capture state machine. No Android dependency, so they
 * run on the standard JUnit task.
 */
class CaptureStateMachineTest {

    private fun reduce(state: CaptureState, vararg events: CaptureEvent): CaptureState {
        var current = state
        for (e in events) current = CaptureStateMachine.reduce(current, e)
        return current
    }

    @Test
    fun happyPath_idleToRunningAndBack() {
        val running = reduce(
            CaptureState.Idle,
            CaptureEvent.StartRequested,
            CaptureEvent.ConsentGranted,
            CaptureEvent.EngineStarted,
        )
        assertThat(running).isEqualTo(CaptureState.Running())

        val idle = reduce(
            running,
            CaptureEvent.StopRequested,
            CaptureEvent.EngineStopped,
        )
        assertThat(idle).isEqualTo(CaptureState.Idle)
    }

    @Test
    fun consentDenied_returnsToIdle() {
        val state = reduce(
            CaptureState.Idle,
            CaptureEvent.StartRequested,
            CaptureEvent.ConsentDenied,
        )
        assertThat(state).isEqualTo(CaptureState.Idle)
    }

    @Test
    fun failureFromAnyActiveState_goesToError() {
        val fromStarting = reduce(
            CaptureState.Idle,
            CaptureEvent.StartRequested,
            CaptureEvent.ConsentGranted,
            CaptureEvent.Failed("boom"),
        )
        assertThat(fromStarting).isEqualTo(CaptureState.Error("boom"))
    }

    @Test
    fun errorCanRestart() {
        val state = reduce(
            CaptureState.Error("x"),
            CaptureEvent.StartRequested,
        )
        assertThat(state).isEqualTo(CaptureState.AwaitingConsent)
    }

    @Test
    fun errorCanBeDismissed() {
        assertThat(CaptureStateMachine.reduce(CaptureState.Error("x"), CaptureEvent.Dismissed))
            .isEqualTo(CaptureState.Idle)
    }

    @Test
    fun stopRequestFromRunning_movesToStopping() {
        assertThat(CaptureStateMachine.reduce(CaptureState.Running(), CaptureEvent.StopRequested))
            .isEqualTo(CaptureState.Stopping)
    }

    @Test
    fun stopRequestWhileIdle_isNoOp() {
        assertThat(CaptureStateMachine.reduce(CaptureState.Idle, CaptureEvent.StopRequested))
            .isEqualTo(CaptureState.Idle)
    }

    @Test
    fun sourceChange_updatesRunningLabel() {
        val labeled = CaptureStateMachine.reduce(
            CaptureState.Running(),
            CaptureEvent.SourceChanged("YouTube"),
        )
        assertThat(labeled).isEqualTo(CaptureState.Running("YouTube"))
    }

    @Test
    fun unexpectedEvent_isIgnored() {
        // EngineStarted has no meaning while Idle; state must not change.
        assertThat(CaptureStateMachine.reduce(CaptureState.Idle, CaptureEvent.EngineStarted))
            .isEqualTo(CaptureState.Idle)
    }

    @Test
    fun duplicateEngineStopped_isIdempotent() {
        val once = CaptureStateMachine.reduce(CaptureState.Stopping, CaptureEvent.EngineStopped)
        val twice = CaptureStateMachine.reduce(once, CaptureEvent.EngineStopped)
        assertThat(once).isEqualTo(CaptureState.Idle)
        assertThat(twice).isEqualTo(CaptureState.Idle)
    }
}
