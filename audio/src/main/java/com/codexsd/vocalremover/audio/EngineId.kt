package com.codexsd.vocalremover.audio

/**
 * Stable engine identifiers, shared one-to-one with the native registry keys in
 * `EngineRegistry.cpp`. The Kotlin layer decides which id to use; native only
 * looks it up.
 */
enum class EngineId(val nativeId: String) {
    /** ONNX/ML separator (Band-SCNet). Requires a bundled model. */
    ML("ml-bandscnet"),

    /** DSP center extraction. The default; needs no model. */
    DSP("dsp-center"),

    /** Identity. Terminal fallback / diagnostic. */
    PASSTHROUGH("passthrough");

    companion object {
        fun fromNativeId(id: String): EngineId? = entries.firstOrNull { it.nativeId == id }
    }
}

/** Config keys understood by the native engines (see `EngineRegistry.cpp`). */
object EngineParams {
    const val DSP_SHARPNESS = "dsp.center.sharpness"
    const val DSP_FLOOR = "dsp.center.floor"
    const val DSP_FFT_SIZE = "dsp.fftSize"
    const val DSP_HOP = "dsp.hop"
}
