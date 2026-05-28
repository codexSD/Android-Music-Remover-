package com.codexsd.vocalremover.audio

import androidx.test.ext.junit.runners.AndroidJUnit4
import com.google.common.truth.Truth.assertThat
import org.junit.Test
import org.junit.runner.RunWith

/**
 * Device/emulator-required test (runs via `./gradlew :audio:connectedCheck`).
 *
 * It exercises the JNI boundary without needing MediaProjection consent:
 * constructing [AudioEngine] forces System.loadLibrary + nativeCreate, and
 * [AudioEngine.release] forces nativeDestroy. If the .so is missing, an ABI is
 * misconfigured, or a JNI symbol name is wrong, this fails to even load.
 */
@RunWith(AndroidJUnit4::class)
class AudioEngineLifecycleTest {

    @Test
    fun createAndRelease_doesNotCrash() {
        val engine = AudioEngine()
        try {
            assertThat(engine.isRunning).isFalse()
        } finally {
            engine.release()
        }
    }

    @Test
    fun statsBeforeStart_areZero() {
        val engine = AudioEngine()
        try {
            val stats = engine.stats()
            assertThat(stats.framesCaptured).isEqualTo(0)
            assertThat(stats.framesDropped).isEqualTo(0)
            assertThat(stats.underrunFrames).isEqualTo(0)
        } finally {
            engine.release()
        }
    }

    @Test
    fun releaseIsIdempotent() {
        val engine = AudioEngine()
        engine.release()
        engine.release() // second call must be a no-op, not a double-free
    }
}
