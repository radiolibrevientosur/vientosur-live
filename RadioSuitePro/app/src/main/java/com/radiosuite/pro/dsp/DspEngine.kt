package com.radiosuite.pro.dsp

/**
 * Puente JNI sobre el motor DSP nativo (C++/NDK).
 *
 * Cada instancia posee un handle nativo (long) que identifica al [NativeEngine]
 * C++ subyacente: mezclador + pipeline DSP + render Oboe + codificador.
 *
 * Thread-safety: los métodos de parámetros son seguros desde cualquier hilo;
 * los de métricas están pensados para polling desde la UI (StateFlow).
 */
class DspEngine {

    private var handle: Long = 0L
    private var released = false

    init {
        handle = nativeCreate()
        require(handle != 0L) { "No se pudo inicializar el motor DSP nativo" }
    }

    /** Inicia el render Oboe + pipeline. */
    fun start(): Boolean = nativeStart(handle)

    /** Detiene el render y libera el stream Oboe. */
    fun stop() = nativeStop(handle)

    /** Publica una nueva instantánea de parámetros del pipeline. */
    fun updatePipeline(params: DspParameters) {
        nativeUpdatePipeline(
            handle,
            params.agcTargetDb,
            params.agcMaxGainDb,
            params.agcAttackMs,
            params.agcReleaseMs,
            params.outputGainDb,
            params.enabled
        )
    }

    // ---- Métricas para vómetros en tiempo real ----
    fun inputPeakDb(): Float  = nativeInputPeakDb(handle)
    fun outputPeakDb(): Float = nativeOutputPeakDb(handle)
    fun outputRmsDb(): Float  = nativeOutputRmsDb(handle)
    fun agcGainDb(): Float    = nativeAgcGainDb(handle)
    fun brickwallGrDb(): Float = nativeBrickwallGrDb(handle)

    @Synchronized
    fun release() {
        if (!released && handle != 0L) {
            nativeDestroy(handle)
            handle = 0L
            released = true
        }
    }

    protected fun finalize() = release()

    // ---- Declaraciones nativas (jni_bridge.cpp) ----
    private external companion object {
        @JvmStatic fun nativeCreate(): Long
        @JvmStatic fun nativeDestroy(handle: Long)
        @JvmStatic fun nativeStart(handle: Long): Boolean
        @JvmStatic fun nativeStop(handle: Long)
        @JvmStatic fun nativeUpdatePipeline(
            handle: Long, targetDb: Float, maxGainDb: Float,
            agcAttack: Float, agcRelease: Float,
            outGainDb: Float, enabled: Boolean
        )
        @JvmStatic fun nativeInputPeakDb(handle: Long): Float
        @JvmStatic fun nativeOutputPeakDb(handle: Long): Float
        @JvmStatic fun nativeOutputRmsDb(handle: Long): Float
        @JvmStatic fun nativeAgcGainDb(handle: Long): Float
        @JvmStatic fun nativeBrickwallGrDb(handle: Long): Float
    }

    init {
        System.loadLibrary("radiosuite-native")
    }
}
