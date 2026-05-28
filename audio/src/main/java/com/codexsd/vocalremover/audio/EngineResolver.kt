package com.codexsd.vocalremover.audio

/** A concrete engine choice: which engine, with which tunables. */
data class EnginePlan(val id: EngineId, val params: Map<String, String> = emptyMap())

/**
 * Decides which engine to run and in what fallback order. This is the ONLY place
 * engine selection lives; the native pipeline receives a decided plan and never
 * chooses. Pure (no Android APIs) so it is unit tested directly.
 *
 * Precedence for the chain head:
 *   1. explicit user override (from Settings), else
 *   2. device-tier heuristic: HIGH tier + model available -> ML, otherwise DSP.
 *
 * The remainder is a data-driven canonical order (ML -> DSP -> PASSTHROUGH),
 * deduplicated with the head first, with ML removed when no model is available.
 * The caller walks this list, advancing on init failure or deadline violation.
 */
class EngineResolver(
    private val override: EngineId? = null,
    private val modelAvailable: Boolean = false,
) {
    fun chain(facts: DeviceFacts): List<EnginePlan> {
        val head = override ?: defaultFor(facts)
        val ordered = (listOf(head) + CANONICAL_ORDER)
            .distinct()
            .filter { it != EngineId.ML || modelAvailable }
        return ordered.map { EnginePlan(it, paramsFor(it)) }
    }

    private fun defaultFor(facts: DeviceFacts): EngineId =
        if (modelAvailable && DeviceTier.classify(facts) == DeviceTier.Tier.HIGH) {
            EngineId.ML
        } else {
            EngineId.DSP
        }

    private fun paramsFor(id: EngineId): Map<String, String> = when (id) {
        EngineId.DSP -> mapOf(EngineParams.DSP_SHARPNESS to "2.0")
        else -> emptyMap()
    }

    private companion object {
        val CANONICAL_ORDER = listOf(EngineId.ML, EngineId.DSP, EngineId.PASSTHROUGH)
    }
}
