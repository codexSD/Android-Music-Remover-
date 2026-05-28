package com.codexsd.vocalremover.audio

import com.google.common.truth.Truth.assertThat
import org.junit.Test

class EngineResolverTest {

    private val highTier = DeviceFacts(cores = 8, sdkInt = 33, totalRamMb = 8_000, abiSupports64 = true)
    private val lowTier = DeviceFacts(cores = 4, sdkInt = 29, totalRamMb = 3_000, abiSupports64 = true)

    private fun ids(plans: List<EnginePlan>) = plans.map { it.id }

    @Test
    fun highTierWithModelHeadsWithMl() {
        val chain = EngineResolver(override = null, modelAvailable = true).chain(highTier)
        assertThat(ids(chain))
            .containsExactly(EngineId.ML, EngineId.DSP, EngineId.PASSTHROUGH).inOrder()
    }

    @Test
    fun lowTierHeadsWithDsp() {
        val chain = EngineResolver(override = null, modelAvailable = true).chain(lowTier)
        assertThat(ids(chain))
            .containsExactly(EngineId.DSP, EngineId.ML, EngineId.PASSTHROUGH).inOrder()
    }

    @Test
    fun noModelDropsMlEntirely() {
        val chain = EngineResolver(override = null, modelAvailable = false).chain(highTier)
        assertThat(ids(chain)).containsExactly(EngineId.DSP, EngineId.PASSTHROUGH).inOrder()
        assertThat(ids(chain)).doesNotContain(EngineId.ML)
    }

    @Test
    fun explicitOverrideWins() {
        val chain = EngineResolver(override = EngineId.PASSTHROUGH, modelAvailable = true)
            .chain(highTier)
        assertThat(chain.first().id).isEqualTo(EngineId.PASSTHROUGH)
        // Remaining fallbacks still follow, deduped.
        assertThat(ids(chain))
            .containsExactly(EngineId.PASSTHROUGH, EngineId.ML, EngineId.DSP).inOrder()
    }

    @Test
    fun overrideMlWithoutModelIsDroppedLeavingDsp() {
        val chain = EngineResolver(override = EngineId.ML, modelAvailable = false).chain(highTier)
        // ML filtered out despite the override, since there's no model.
        assertThat(ids(chain)).containsExactly(EngineId.DSP, EngineId.PASSTHROUGH).inOrder()
    }

    @Test
    fun dspPlanCarriesSharpnessParam() {
        val chain = EngineResolver(override = EngineId.DSP, modelAvailable = false).chain(lowTier)
        val dsp = chain.first { it.id == EngineId.DSP }
        assertThat(dsp.params).containsKey(EngineParams.DSP_SHARPNESS)
    }
}
