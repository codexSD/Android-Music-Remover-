package com.codexsd.vocalremover.audio

import android.media.AudioFormat

/**
 * Canonical audio format for the capture/playback pipeline, shared by the
 * capture service (which builds the AudioRecord) and the native engine.
 *
 * The pipeline runs at 48 kHz stereo float — the native rate for media on
 * modern devices, which avoids a resample on the hot path.
 */
object AudioFormatSpec {
    const val SAMPLE_RATE = 48_000
    const val CHANNEL_COUNT = 2

    // Not `const`: these are initialized from Java static fields, which Kotlin
    // does not treat as compile-time constants.
    val ENCODING = AudioFormat.ENCODING_PCM_FLOAT
    val CHANNEL_MASK = AudioFormat.CHANNEL_IN_STEREO

    /** Bytes per sample for [ENCODING] (PCM float = 4 bytes). */
    const val BYTES_PER_SAMPLE = 4

    /**
     * Headroom multiplier applied to AudioRecord.getMinBufferSize(). A larger
     * record buffer tolerates scheduling jitter before the native capture
     * thread drains it; 4x is the value the Phase 1 plan calls for.
     */
    const val RECORD_BUFFER_MULTIPLIER = 4

    /**
     * Pure helper so the buffer-sizing policy can be unit tested without an
     * Android runtime. Guards against the error sentinels getMinBufferSize()
     * can return (ERROR == -1, ERROR_BAD_VALUE == -2) by falling back to a
     * one-second buffer.
     */
    fun recordBufferSizeBytes(minBufferSizeBytes: Int): Int {
        if (minBufferSizeBytes <= 0) {
            return SAMPLE_RATE * CHANNEL_COUNT * BYTES_PER_SAMPLE
        }
        return minBufferSizeBytes * RECORD_BUFFER_MULTIPLIER
    }
}
