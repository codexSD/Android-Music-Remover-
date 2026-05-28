package com.codexsd.vocalremover.audio

import android.media.AudioRecord
import androidx.annotation.AnyThread

/**
 * Kotlin handle to the native audio engine.
 *
 * The engine owns the capture thread and the Oboe output stream; this class is
 * just a thin lifecycle wrapper over the JNI boundary. [start] hands the
 * configured [AudioRecord] (created by the caller, who holds the
 * MediaProjection) down to native code, which reads from it on its own thread.
 *
 * Not thread-safe: call [start]/[stop]/[release] from a single owner (the
 * capture service).
 */
class AudioEngine {

    private var handle: Long = nativeCreate()
    private var running = false

    val isRunning: Boolean get() = running

    /**
     * Starts capture and low-latency playback with the chosen engine. Returns
     * true on success, false if the engine is unknown or fails to initialize
     * (the caller's resolver then tries the next engine in its fallback chain).
     *
     * Changing engine at runtime is done by [stop] then [start] again with a new
     * [engineId]; there is no live swap.
     *
     * @param audioRecord an AudioRecord configured for [AudioFormatSpec] (PCM
     *   float, [sampleRate], [channelCount]). Ownership of start/stop is handed
     *   to native code; the caller must not also call startRecording()/stop().
     * @param engineId native engine id (see [EngineId]): "dsp-center" (default),
     *   "ml-bandscnet", or "passthrough".
     * @param config small per-engine tunables (see [EngineId] param keys).
     * @param model raw bytes of an ONNX model for the ML engine, or null.
     */
    fun start(
        audioRecord: AudioRecord,
        sampleRate: Int,
        channelCount: Int,
        engineId: String,
        config: Map<String, String> = emptyMap(),
        model: ByteArray? = null,
    ): Boolean {
        check(handle != 0L) { "AudioEngine already released" }
        if (running) return false
        val keys = config.keys.toTypedArray()
        val values = Array(keys.size) { config.getValue(keys[it]) }
        running = nativeStart(
            handle, audioRecord, sampleRate, channelCount, engineId, keys, values, model,
        )
        return running
    }

    fun stop() {
        if (handle == 0L || !running) return
        nativeStop(handle)
        running = false
    }

    /** Snapshot of pipeline counters, useful for diagnostics and the Phase 1 gate. */
    @AnyThread
    fun stats(): EngineStats {
        if (handle == 0L) return EngineStats()
        return EngineStats(
            framesCaptured = nativeFramesCaptured(handle),
            framesDropped = nativeFramesDropped(handle),
            underrunFrames = nativeUnderrunFrames(handle),
            captureRms = nativeCaptureRms(handle),
            deadlineViolations = nativeDeadlineViolations(handle),
        )
    }

    /** Releases native resources. The instance must not be used afterwards. */
    fun release() {
        if (handle == 0L) return
        if (running) {
            nativeStop(handle)
            running = false
        }
        nativeDestroy(handle)
        handle = 0L
    }

    private external fun nativeCreate(): Long
    private external fun nativeStart(
        handle: Long,
        audioRecord: AudioRecord,
        sampleRate: Int,
        channelCount: Int,
        engineId: String,
        configKeys: Array<String>,
        configValues: Array<String>,
        model: ByteArray?,
    ): Boolean

    private external fun nativeStop(handle: Long)
    private external fun nativeDestroy(handle: Long)
    private external fun nativeFramesCaptured(handle: Long): Long
    private external fun nativeFramesDropped(handle: Long): Long
    private external fun nativeUnderrunFrames(handle: Long): Long
    private external fun nativeCaptureRms(handle: Long): Float
    private external fun nativeDeadlineViolations(handle: Long): Long

    companion object {
        init {
            System.loadLibrary("vocalremover_audio")
        }
    }
}

/** Immutable snapshot of native pipeline counters. */
data class EngineStats(
    val framesCaptured: Long = 0,
    val framesDropped: Long = 0,
    val underrunFrames: Long = 0,
    /** RMS of the most recent captured block, in [0, 1]. */
    val captureRms: Float = 0f,
    /** Count of sustained deadline-overrun events (engine too slow). */
    val deadlineViolations: Long = 0,
)
