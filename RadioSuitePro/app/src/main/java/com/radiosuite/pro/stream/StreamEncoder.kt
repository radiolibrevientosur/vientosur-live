package com.radiosuite.pro.stream

/**
 * Codec de emisión soportado por el codificador nativo (C++).
 */
enum class StreamCodec(val nativeId: Int) {
    Opus(0),
    Vorbis(1),
    Mp3(2);

    companion object {
        fun fromNative(id: Int): StreamCodec = entries.firstOrNull { it.nativeId == id } ?: Opus
    }
}

/**
 * Puente JNI sobre el codificador de streaming nativo (IcecastEncoder.cpp).
 * El pipeline DSP alimenta el codificador; los paquetes resultantes se envían
 * a Icecast/Shoutcast mediante [IcecastClient].
 */
class StreamEncoder(private val engineHandle: Long) {

    private var started = false

    fun start(codec: StreamCodec, sampleRate: Int, channels: Int, bitrate: Int): Boolean {
        started = nativeStartEncoder(engineHandle, codec.nativeId, sampleRate, channels, bitrate)
        return started
    }

    fun stop() {
        if (started) {
            nativeStopEncoder(engineHandle)
            started = false
        }
    }

    private external companion object {
        @JvmStatic fun nativeStartEncoder(
            handle: Long, codec: Int, sampleRate: Int, channels: Int, bitrate: Int
        ): Boolean
        @JvmStatic fun nativeStopEncoder(handle: Long)
    }

    init {
        System.loadLibrary("radiosuite-native")
    }
}
