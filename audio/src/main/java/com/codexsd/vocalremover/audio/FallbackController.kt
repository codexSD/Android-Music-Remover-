package com.codexsd.vocalremover.audio

/**
 * Walks an engine fallback chain. Pure state (just an index), so the
 * advance-on-failure logic is unit tested without Android.
 *
 * The caller starts at [current], tries to start that engine, and:
 *  - on init failure, calls [advance] to get the next plan and retries, or
 *  - while running, on a deadline violation, calls [advance] to pick the next
 *    plan and performs a stop/restart with it.
 *
 * [advance] returns null when the chain is exhausted (already at the terminal,
 * typically passthrough), meaning there is nothing safer to fall back to.
 */
class FallbackController(private val chain: List<EnginePlan>) {
    init {
        require(chain.isNotEmpty()) { "fallback chain must not be empty" }
    }

    private var index = 0

    val current: EnginePlan get() = chain[index]

    /** Moves to the next engine in the chain, or null if already at the end. */
    fun advance(): EnginePlan? {
        if (index >= chain.lastIndex) return null
        index++
        return chain[index]
    }

    val isAtTerminal: Boolean get() = index >= chain.lastIndex
}
