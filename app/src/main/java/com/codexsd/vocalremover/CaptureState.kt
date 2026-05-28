package com.codexsd.vocalremover

/**
 * The lifecycle of a capture session, kept as a pure state machine so the
 * transition logic can be reasoned about and unit tested independently of the
 * Android framework. [MainActivity] and [CaptureService] drive it by feeding
 * [CaptureEvent]s through [CaptureStateMachine.reduce].
 */
sealed interface CaptureState {
    /** Nothing running; the start button is enabled. */
    data object Idle : CaptureState

    /** The MediaProjection consent dialog is showing. */
    data object AwaitingConsent : CaptureState

    /** Consent granted; the service is spinning up capture + output. */
    data object Starting : CaptureState

    /** Audio is flowing. [sourceLabel] is the foreground media app, if known. */
    data class Running(val sourceLabel: String? = null) : CaptureState

    /** Tearing down. */
    data object Stopping : CaptureState

    /** Something failed; [message] is user-presentable. */
    data class Error(val message: String) : CaptureState
}

sealed interface CaptureEvent {
    /** User tapped start. */
    data object StartRequested : CaptureEvent

    /** MediaProjection consent dialog returned. */
    data object ConsentGranted : CaptureEvent
    data object ConsentDenied : CaptureEvent

    /** Native engine reported it is running. */
    data object EngineStarted : CaptureEvent

    /** Detected foreground media source changed. */
    data class SourceChanged(val label: String?) : CaptureEvent

    /** User tapped stop (in app or notification). */
    data object StopRequested : CaptureEvent

    /** Native engine fully torn down. */
    data object EngineStopped : CaptureEvent

    /** A failure occurred at any point. */
    data class Failed(val message: String) : CaptureEvent

    /** Dismiss an error back to idle. */
    data object Dismissed : CaptureEvent
}

object CaptureStateMachine {

    /**
     * Computes the next state. Unhandled (state, event) pairs are treated as
     * no-ops and return the current state unchanged, which keeps the UI robust
     * against duplicate or late events (e.g. a second EngineStopped).
     */
    fun reduce(state: CaptureState, event: CaptureEvent): CaptureState {
        // A failure or stop request can arrive in almost any active state, so
        // handle them first regardless of the current state.
        when (event) {
            is CaptureEvent.Failed -> return CaptureState.Error(event.message)
            is CaptureEvent.StopRequested ->
                return if (state is CaptureState.Idle || state is CaptureState.Error) {
                    state
                } else {
                    CaptureState.Stopping
                }
            else -> Unit
        }

        return when (state) {
            is CaptureState.Idle -> when (event) {
                is CaptureEvent.StartRequested -> CaptureState.AwaitingConsent
                else -> state
            }

            is CaptureState.AwaitingConsent -> when (event) {
                is CaptureEvent.ConsentGranted -> CaptureState.Starting
                is CaptureEvent.ConsentDenied -> CaptureState.Idle
                else -> state
            }

            is CaptureState.Starting -> when (event) {
                is CaptureEvent.EngineStarted -> CaptureState.Running()
                else -> state
            }

            is CaptureState.Running -> when (event) {
                is CaptureEvent.SourceChanged -> CaptureState.Running(event.label)
                else -> state
            }

            is CaptureState.Stopping -> when (event) {
                is CaptureEvent.EngineStopped -> CaptureState.Idle
                else -> state
            }

            is CaptureState.Error -> when (event) {
                is CaptureEvent.Dismissed -> CaptureState.Idle
                is CaptureEvent.StartRequested -> CaptureState.AwaitingConsent
                else -> state
            }
        }
    }
}
