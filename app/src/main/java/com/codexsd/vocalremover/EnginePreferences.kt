package com.codexsd.vocalremover

import android.content.Context
import com.codexsd.vocalremover.audio.EngineId

/**
 * Persists the user's engine choice from the Settings screen. "Auto" (null
 * override) lets the resolver pick by device tier; an explicit choice forces
 * that engine. Because engine changes require a restart, a new choice takes
 * effect on the next capture session.
 */
object EnginePreferences {
    private const val PREFS = "engine_prefs"
    private const val KEY_OVERRIDE = "engine_override"
    private const val AUTO = "AUTO"

    /** Returns the forced engine, or null for Auto (resolver decides). */
    fun getOverride(context: Context): EngineId? {
        val stored = prefs(context).getString(KEY_OVERRIDE, AUTO) ?: AUTO
        if (stored == AUTO) return null
        return runCatching { EngineId.valueOf(stored) }.getOrNull()
    }

    fun setOverride(context: Context, id: EngineId?) {
        prefs(context).edit().putString(KEY_OVERRIDE, id?.name ?: AUTO).apply()
    }

    private fun prefs(context: Context) =
        context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
}
