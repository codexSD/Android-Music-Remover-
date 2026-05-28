package com.codexsd.vocalremover.audio

import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.google.common.truth.Truth.assertThat
import org.junit.After
import org.junit.Assume.assumeTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

/**
 * Device/emulator test for the production engine-change path: stop the running
 * pipeline, then start again with a different engine id (there is no live swap).
 *
 * Uses a microphone AudioRecord rather than AudioPlaybackCapture so it needs no
 * MediaProjection consent. Requires RECORD_AUDIO; skips if a float-PCM
 * AudioRecord can't be created on this device. The point is to prove that the
 * registry start/stop/restart cycle across engine ids doesn't crash the JNI
 * boundary — not to assert audio content.
 */
@RunWith(AndroidJUnit4::class)
class EngineRestartTest {

    private var record: AudioRecord? = null
    private var engine: AudioEngine? = null

    @Before
    fun setUp() {
        val minBuf = AudioRecord.getMinBufferSize(
            SAMPLE_RATE, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_FLOAT,
        )
        assumeTrue("getMinBufferSize unsupported", minBuf > 0)

        val rec = try {
            AudioRecord.Builder()
                .setAudioSource(MediaRecorder.AudioSource.MIC)
                .setAudioFormat(
                    AudioFormat.Builder()
                        .setEncoding(AudioFormat.ENCODING_PCM_FLOAT)
                        .setSampleRate(SAMPLE_RATE)
                        .setChannelMask(AudioFormat.CHANNEL_IN_MONO)
                        .build(),
                )
                .setBufferSizeInBytes(minBuf * 4)
                .build()
        } catch (e: Exception) {
            null
        }
        assumeTrue("Could not create float AudioRecord (or RECORD_AUDIO denied)",
            rec != null && rec.state == AudioRecord.STATE_INITIALIZED)
        record = rec
    }

    @After
    fun tearDown() {
        engine?.release()
        record?.release()
    }

    @Test
    fun startStopRestartAcrossEngines() {
        val rec = record!!
        val eng = AudioEngine().also { engine = it }

        // Start passthrough.
        assertThat(eng.start(rec, SAMPLE_RATE, 1, EngineId.PASSTHROUGH.nativeId)).isTrue()
        assertThat(eng.isRunning).isTrue()

        // Stop, then restart with the DSP engine (the production engine-change path).
        eng.stop()
        assertThat(eng.isRunning).isFalse()
        assertThat(eng.start(rec, SAMPLE_RATE, 1, EngineId.DSP.nativeId)).isTrue()
        assertThat(eng.isRunning).isTrue()

        eng.stop()
        assertThat(eng.isRunning).isFalse()
    }

    @Test
    fun unknownEngineFailsToStart() {
        val rec = record!!
        val eng = AudioEngine().also { engine = it }
        assertThat(eng.start(rec, SAMPLE_RATE, 1, "no-such-engine")).isFalse()
        assertThat(eng.isRunning).isFalse()
    }

    private companion object {
        const val SAMPLE_RATE = 48_000
    }
}
