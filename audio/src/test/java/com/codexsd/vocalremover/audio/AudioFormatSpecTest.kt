package com.codexsd.vocalremover.audio

import com.google.common.truth.Truth.assertThat
import org.junit.Test

class AudioFormatSpecTest {

    @Test
    fun appliesHeadroomMultiplierToValidMinBuffer() {
        val min = 7680 // e.g. one valid min buffer size in bytes
        assertThat(AudioFormatSpec.recordBufferSizeBytes(min))
            .isEqualTo(min * AudioFormatSpec.RECORD_BUFFER_MULTIPLIER)
    }

    @Test
    fun fallsBackToOneSecondOnErrorSentinel() {
        // getMinBufferSize returns ERROR (-1) or ERROR_BAD_VALUE (-2) on failure.
        val expectedOneSecond =
            AudioFormatSpec.SAMPLE_RATE * AudioFormatSpec.CHANNEL_COUNT *
                AudioFormatSpec.BYTES_PER_SAMPLE
        assertThat(AudioFormatSpec.recordBufferSizeBytes(-1)).isEqualTo(expectedOneSecond)
        assertThat(AudioFormatSpec.recordBufferSizeBytes(-2)).isEqualTo(expectedOneSecond)
        assertThat(AudioFormatSpec.recordBufferSizeBytes(0)).isEqualTo(expectedOneSecond)
    }

    @Test
    fun oneSecondFallbackIsByteCountForFloatStereo48k() {
        // 48000 * 2 * 4 = 384000 bytes.
        assertThat(AudioFormatSpec.recordBufferSizeBytes(0)).isEqualTo(384_000)
    }
}
