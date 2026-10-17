package com.radiosuite.pro.service

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.IBinder
import android.util.Log
import com.radiosuite.pro.dsp.DspEngine
import com.radiosuite.pro.dsp.DspParameters
import com.radiosuite.pro.player.PlaylistManager
import com.radiosuite.pro.stream.IcecastClient
import com.radiosuite.pro.stream.StreamCodec
import com.radiosuite.pro.stream.StreamEncoder
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

/**
 * RadioEngineService
 * ------------------
 * Foreground Service que orquesta los tres motores de la aplicación:
 *
 *   1. Reproductor / PlaylistManager — decodifica pistas y alimenta PCM al mezclador.
 *   2. Motor DSP (C++/NDK)           — pipeline de procesamiento de audio en tiempo real.
 *   3. Streamer (IcecastClient + StreamEncoder) — codifica y emite al servidor.
 *
 * Se ejecuta como foreground service con tipo "mediaPlayback" + "microphone"
 * para garantizar operación 24/7 sin que el sistema mate el proceso cuando la
 * app pasa a segundo plano o la pantalla se apaga.
 *
 * La UI (Jetpack Compose) se comunica con el servicio vía un binder expuesto
 * (RadioEngineBinder) que da acceso a [engine] (DSP), [playlist] y [streamer].
 */
class RadioEngineService : Service() {

    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Default)
    private var meterJob: Job? = null

    // Motores. Inicializados en onCreate y liberados en onDestroy.
    lateinit var engine: DspEngine
        private set
    lateinit var playlist: PlaylistManager
        private set
    private var streamEncoder: StreamEncoder? = null
    private var icecast: IcecastClient? = null

    private val binder = RadioEngineBinder()

    inner class RadioEngineBinder : android.os.Binder() {
        val service: RadioEngineService get() = this@RadioEngineService
    }

    override fun onCreate() {
        super.onCreate()
        engine = DspEngine().also { it.start() }
        playlist = PlaylistManager()
        Log.i(TAG, "Motores DSP + reproductor inicializados")
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        startForegroundCompat()
        when (intent?.action) {
            ACTION_START_STREAM -> startStreaming(intent)
            ACTION_STOP_STREAM  -> stopStreaming()
            ACTION_UPDATE_DSP   -> applyDspParameters(intent)
        }
        return START_STICKY  // el sistema reinicia el servicio si lo mata
    }

    // ---------- Streaming ----------
    private fun startStreaming(intent: Intent) {
        if (streamEncoder != null) return
        val codec = StreamCodec.fromNative(intent.getIntExtra(EXTRA_CODEC, 0))
        val sr = intent.getIntExtra(EXTRA_SAMPLE_RATE, 48000)
        val ch = intent.getIntExtra(EXTRA_CHANNELS, 2)
        val bitrate = intent.getIntExtra(EXTRA_BITRATE, 128)
        val host = intent.getStringExtra(EXTRA_HOST) ?: return
        val port = intent.getIntExtra(EXTRA_PORT, 8000)
        val mount = intent.getStringExtra(EXTRA_MOUNT) ?: "/live"
        val password = intent.getStringExtra(EXTRA_PASSWORD) ?: ""

        streamEncoder = StreamEncoder(engine.handle)
        icecast = IcecastClient(host, port, mount, password, codec, bitrate).also { it.start() }
        streamEncoder?.start(codec, sr, ch, bitrate)
        Log.i(TAG, "Streaming iniciado hacia ${host}:${port}${mount} (${codec}, ${bitrate}kbps)")
    }

    private fun stopStreaming() {
        streamEncoder?.stop()
        icecast?.stop()
        streamEncoder = null
        icecast = null
        Log.i(TAG, "Streaming detenido")
    }

    // ---------- DSP ----------
    private fun applyDspParameters(intent: Intent) {
        val p = DspParameters(
            enabled = intent.getBooleanExtra(EXTRA_DSP_ENABLED, true),
            agcTargetDb = intent.getFloatExtra(EXTRA_AGC_TARGET, -18f),
            agcMaxGainDb = intent.getFloatExtra(EXTRA_AGC_MAX_GAIN, 24f),
            agcAttackMs = intent.getFloatExtra(EXTRA_AGC_ATTACK, 150f),
            agcReleaseMs = intent.getFloatExtra(EXTRA_AGC_RELEASE, 800f),
            outputGainDb = intent.getFloatExtra(EXTRA_OUT_GAIN, 0f),
        )
        engine.updatePipeline(p)
    }

    // ---------- Métricas para vómetros ----------
    fun startMeterPolling(onTick: (Float, Float, Float, Float) -> Unit) {
        meterJob?.cancel()
        meterJob = scope.launch {
            while (isActive) {
                onTick(
                    engine.inputPeakDb(),
                    engine.outputPeakDb(),
                    engine.outputRmsDb(),
                    engine.agcGainDb(),
                )
                delay(METER_INTERVAL_MS)
            }
        }
    }

    fun stopMeterPolling() { meterJob?.cancel(); meterJob = null }

    // ---------- Foreground ----------
    private fun startForegroundCompat() {
        val channelId = CHANNEL_ID
        val nm = getSystemService(NotificationManager::class.java)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            nm.createNotificationChannel(NotificationChannel(
                channelId, "Radio Engine", NotificationManager.IMPORTANCE_LOW
            ))
        }
        val pi = PendingIntent.getService(
            this, 0, Intent(), PendingIntent.FLAG_IMMUTABLE
        )
        val notif = Notification.Builder(this, channelId)
            .setContentTitle("RadioSuite Pro")
            .setContentText("Emisión en curso")
            .setSmallIcon(android.R.drawable.ic_media_play)
            .setContentIntent(pi)
            .setOngoing(true)
            .build()

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            startForeground(NOTIF_ID, notif,
                ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK or
                ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE)
        } else {
            startForeground(NOTIF_ID, notif)
        }
    }

    override fun onDestroy() {
        stopMeterPolling()
        stopStreaming()
        engine.stop()
        engine.release()
        scope.cancel()
        Log.i(TAG, "Servicio destruido y motores liberados")
        super.onDestroy()
    }

    override fun onBind(intent: Intent?): IBinder = binder

    companion object {
        private const val TAG = "RadioEngineService"
        private const val NOTIF_ID = 1001
        private const val CHANNEL_ID = "radio_engine"
        private const val METER_INTERVAL_MS = 33L // ~30 fps para vómetros fluidos

        const val ACTION_START_STREAM = "com.radiosuite.pro.START_STREAM"
        const val ACTION_STOP_STREAM  = "com.radiosuite.pro.STOP_STREAM"
        const val ACTION_UPDATE_DSP   = "com.radiosuite.pro.UPDATE_DSP"

        const val EXTRA_HOST = "host"
        const val EXTRA_PORT = "port"
        const val EXTRA_MOUNT = "mount"
        const val EXTRA_PASSWORD = "password"
        const val EXTRA_CODEC = "codec"
        const val EXTRA_SAMPLE_RATE = "sampleRate"
        const val EXTRA_CHANNELS = "channels"
        const val EXTRA_BITRATE = "bitrate"

        const val EXTRA_DSP_ENABLED = "dspEnabled"
        const val EXTRA_AGC_TARGET = "agcTarget"
        const val EXTRA_AGC_MAX_GAIN = "agcMaxGain"
        const val EXTRA_AGC_ATTACK = "agcAttack"
        const val EXTRA_AGC_RELEASE = "agcRelease"
        const val EXTRA_OUT_GAIN = "outGain"

        fun start(context: Context) {
            val intent = Intent(context, RadioEngineService::class.java)
            ContextCompat.startForegroundService(context, intent)
        }
    }
}

private object ContextCompat {
    fun startForegroundService(context: Context, intent: Intent) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            context.startForegroundService(intent)
        } else {
            context.startService(intent)
        }
    }
}
