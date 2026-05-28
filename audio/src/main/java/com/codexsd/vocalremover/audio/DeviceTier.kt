package com.codexsd.vocalremover.audio

/**
 * Facts about the device used to choose a default engine. Gathered by the caller
 * (from Build/ActivityManager) and passed in, so the classifier stays pure and
 * unit-testable.
 */
data class DeviceFacts(
    val cores: Int,
    val sdkInt: Int,
    val totalRamMb: Long,
    val abiSupports64: Boolean,
)

/**
 * Coarse device capability tier. HIGH devices can afford the heavier ML engine;
 * everything else defaults to the lightweight DSP engine.
 */
object DeviceTier {
    enum class Tier { HIGH, LOW }

    fun classify(f: DeviceFacts): Tier =
        if (f.cores >= 8 && f.totalRamMb >= 6_000 && f.sdkInt >= 29 && f.abiSupports64) {
            Tier.HIGH
        } else {
            Tier.LOW
        }
}
