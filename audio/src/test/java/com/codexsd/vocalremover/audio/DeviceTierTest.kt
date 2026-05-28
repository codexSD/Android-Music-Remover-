package com.codexsd.vocalremover.audio

import com.google.common.truth.Truth.assertThat
import org.junit.Test

class DeviceTierTest {

    private fun facts(
        cores: Int = 8,
        sdkInt: Int = 33,
        ramMb: Long = 8_000,
        abi64: Boolean = true,
    ) = DeviceFacts(cores, sdkInt, ramMb, abi64)

    @Test
    fun flagshipIsHigh() {
        assertThat(DeviceTier.classify(facts())).isEqualTo(DeviceTier.Tier.HIGH)
    }

    @Test
    fun tooFewCoresIsLow() {
        assertThat(DeviceTier.classify(facts(cores = 7))).isEqualTo(DeviceTier.Tier.LOW)
    }

    @Test
    fun lowRamIsLow() {
        assertThat(DeviceTier.classify(facts(ramMb = 4_000))).isEqualTo(DeviceTier.Tier.LOW)
    }

    @Test
    fun oldSdkIsLow() {
        assertThat(DeviceTier.classify(facts(sdkInt = 28))).isEqualTo(DeviceTier.Tier.LOW)
    }

    @Test
    fun thirtyTwoBitOnlyIsLow() {
        assertThat(DeviceTier.classify(facts(abi64 = false))).isEqualTo(DeviceTier.Tier.LOW)
    }

    @Test
    fun exactThresholdsAreHigh() {
        assertThat(DeviceTier.classify(facts(cores = 8, sdkInt = 29, ramMb = 6_000)))
            .isEqualTo(DeviceTier.Tier.HIGH)
    }
}
