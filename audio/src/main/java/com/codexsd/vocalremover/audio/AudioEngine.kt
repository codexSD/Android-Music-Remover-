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
     * Starts capture and low-latency playback. Returns true on success.
     *
     * @param audioRecord an AudioRecord configured for [AudioFormatSpec] (PCM
     *   float, [sampleRate], [channelCount]). Ownership of start/stop is handed
     *   to native code; the caller must not also call startRecording()/stop().
     */
    fun start(audioRecord: AudioRecord, sampleRate: Int, channelCount: Int): Boolean {
        check(handle != 0L) { "AudioEngine already released" }
        if (running) return false
        running = nativeStart(handle, audioRecord, sampleRate, channelCount)
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
    ): Boolean

    private external fun nativeStop(handle: Long)
    private external fun nativeDestroy(handle: Long)
    private external fun nativeFramesCaptured(handle: Long): Long
    private external fun nativeFramesDropped(handle: Long): Long
    private external fun nativeUnderrunFrames(handle: Long): Long

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
)
