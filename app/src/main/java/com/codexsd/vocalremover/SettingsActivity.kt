package com.codexsd.vocalremover

import android.os.Bundle
import android.widget.RadioGroup
import androidx.appcompat.app.AppCompatActivity
import com.codexsd.vocalremover.audio.EngineId

/**
 * Lets the user choose the isolation engine. "Auto" defers to the resolver's
 * device-tier heuristic; the others force a specific engine. A change applies on
 * the next capture session (engine changes require a restart, never a live swap).
 */
class SettingsActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_settings)

        val group = findViewById<RadioGroup>(R.id.engine_group)
        group.check(checkedIdFor(EnginePreferences.getOverride(this)))
        group.setOnCheckedChangeListener { _, checkedId ->
            EnginePreferences.setOverride(this, overrideFor(checkedId))
        }
    }

    private fun checkedIdFor(id: EngineId?): Int = when (id) {
        EngineId.DSP -> R.id.engine_dsp
        EngineId.ML -> R.id.engine_ml
        EngineId.PASSTHROUGH -> R.id.engine_passthrough
        null -> R.id.engine_auto
    }

    private fun overrideFor(checkedId: Int): EngineId? = when (checkedId) {
        R.id.engine_dsp -> EngineId.DSP
        R.id.engine_ml -> EngineId.ML
        R.id.engine_passthrough -> EngineId.PASSTHROUGH
        else -> null // Auto
    }
}
