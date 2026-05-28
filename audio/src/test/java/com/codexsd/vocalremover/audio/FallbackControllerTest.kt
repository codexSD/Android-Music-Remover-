package com.codexsd.vocalremover.audio

import com.google.common.truth.Truth.assertThat
import org.junit.Test

class FallbackControllerTest {

    private fun chainOf(vararg ids: EngineId) = ids.map { EnginePlan(it) }

    @Test
    fun startsAtHead() {
        val c = FallbackController(chainOf(EngineId.ML, EngineId.DSP, EngineId.PASSTHROUGH))
        assertThat(c.current.id).isEqualTo(EngineId.ML)
        assertThat(c.isAtTerminal).isFalse()
    }

    @Test
    fun advanceWalksTheChain() {
        val c = FallbackController(chainOf(EngineId.ML, EngineId.DSP, EngineId.PASSTHROUGH))
        assertThat(c.advance()?.id).isEqualTo(EngineId.DSP)
        assertThat(c.advance()?.id).isEqualTo(EngineId.PASSTHROUGH)
        assertThat(c.isAtTerminal).isTrue()
    }

    @Test
    fun advancePastTerminalReturnsNull() {
        val c = FallbackController(chainOf(EngineId.DSP, EngineId.PASSTHROUGH))
        c.advance() // -> PASSTHROUGH
        assertThat(c.advance()).isNull()
        // current stays at terminal
        assertThat(c.current.id).isEqualTo(EngineId.PASSTHROUGH)
    }

    @Test
    fun singleElementChainIsImmediatelyTerminal() {
        val c = FallbackController(chainOf(EngineId.DSP))
        assertThat(c.isAtTerminal).isTrue()
        assertThat(c.advance()).isNull()
    }

    @Test(expected = IllegalArgumentException::class)
    fun emptyChainRejected() {
        FallbackController(emptyList())
    }
}
