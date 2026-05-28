package com.codexsd.vocalremover

import android.app.ActivityManager
import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.media.AudioFormat
import android.media.AudioPlaybackCaptureConfiguration
import android.media.AudioRecord
import android.media.AudioDeviceCallback
import android.media.AudioDeviceInfo
import android.media.AudioManager
import android.media.projection.MediaProjection
import android.media.projection.MediaProjectionManager
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.os.SystemClock
import android.util.Log
import androidx.core.app.NotificationCompat
import androidx.core.app.ServiceCompat
import com.codexsd.vocalremover.audio.AudioEngine
import com.codexsd.vocalremover.audio.AudioFormatSpec
import com.codexsd.vocalremover.audio.DeviceFacts
import com.codexsd.vocalremover.audio.EngineId
import com.codexsd.vocalremover.audio.EnginePlan
import com.codexsd.vocalremover.audio.EngineResolver
import com.codexsd.vocalremover.audio.FallbackController

/**
 * Foreground service that owns the capture session.
 *
 * Lifecycle on start:
 *  1. go foreground with the mediaProjection service type (required before
 *     acquiring a MediaProjection on API 29+, strictly enforced on API 34+),
 *  2. build the MediaProjection from the consent result,
 *  3. build an AudioRecord fed by an AudioPlaybackCaptureConfiguration,
 *  4. hand the AudioRecord to the native [AudioEngine].
 */
class CaptureService : Service() {

    private val mainHandler = Handler(Looper.getMainLooper())

    private var mediaProjection: MediaProjection? = null
    private var audioRecord: AudioRecord? = null
    private var engine: AudioEngine? = null

    private var modelBytes: ByteArray? = null
    private var fallback: FallbackController? = null
    private var activeEngine: EngineId? = null

    private val healthMonitor = CaptureHealthMonitor()
    private var lastHealth: CaptureHealth = CaptureHealth.Starting

    private val projectionCallback = object : MediaProjection.Callback() {
        override fun onStop() {
            Log.i(TAG, "MediaProjection stopped by system/user")
            stopCapture()
        }
    }

    // Routing changes (headphones plugged/unplugged, Bluetooth) are handled by
    // Oboe's error/reopen path in native code; we just log them here.
    private val deviceCallback = object : AudioDeviceCallback() {
        override fun onAudioDevicesAdded(added: Array<out AudioDeviceInfo>?) {
            Log.i(TAG, "Audio devices added: ${added?.size ?: 0}")
        }

        override fun onAudioDevicesRemoved(removed: Array<out AudioDeviceInfo>?) {
            Log.i(TAG, "Audio devices removed: ${removed?.size ?: 0}")
        }
    }

    private val healthPoll = object : Runnable {
        override fun run() {
            pollHealth()
            mainHandler.postDelayed(this, POLL_INTERVAL_MS)
        }
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_START -> handleStart(intent)
            ACTION_STOP -> {
                stopCapture()
                return START_NOT_STICKY
            }
        }
        return START_NOT_STICKY
    }

    private fun handleStart(intent: Intent) {
        createNotificationChannel()
        // Must be foreground (mediaProjection type) BEFORE getMediaProjection().
        startForegroundCompat(buildNotification(getString(R.string.status_starting)))

        val resultCode = intent.getIntExtra(EXTRA_RESULT_CODE, 0)
        val resultData: Intent? = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            intent.getParcelableExtra(EXTRA_RESULT_DATA, Intent::class.java)
        } else {
            @Suppress("DEPRECATION")
            intent.getParcelableExtra(EXTRA_RESULT_DATA)
        }
        if (resultData == null) {
            failAndStop("Missing projection consent data")
            return
        }

        val projectionManager =
            getSystemService(Context.MEDIA_PROJECTION_SERVICE) as MediaProjectionManager
        val projection = projectionManager.getMediaProjection(resultCode, resultData)
        if (projection == null) {
            failAndStop("Could not obtain MediaProjection")
            return
        }
        // API 34+ requires a registered callback before capture starts.
        projection.registerCallback(projectionCallback, mainHandler)
        mediaProjection = projection

        if (!startSession(projection)) {
            return
        }

        healthMonitor.reset()
        getSystemService(AudioManager::class.java)
            .registerAudioDeviceCallback(deviceCallback, mainHandler)
        mainHandler.postDelayed(healthPoll, POLL_INTERVAL_MS)
        updateNotification(getString(R.string.status_running))
    }

    private fun pollHealth() {
        val stats = engine?.stats() ?: return

        // Automatic fallback: if the active engine can't keep up, stop and
        // restart with the next engine in the chain (no live swap).
        if (stats.deadlineViolations > 0) {
            val next = fallback?.advance()
            if (next != null) {
                Log.w(TAG, "Deadline violation; falling back to ${next.id}")
                restartWith(next)
                return
            }
        }

        val health = healthMonitor.update(SystemClock.elapsedRealtime(), stats.captureRms)
        if (health == lastHealth) return
        lastHealth = health
        updateNotification(statusTextFor(health))
    }

    private fun statusTextFor(health: CaptureHealth): String = when (health) {
        is CaptureHealth.Blocked -> getString(R.string.status_blocked)
        is CaptureHealth.Healthy ->
            activeEngine?.let { getString(R.string.status_running_engine, it.name) }
                ?: getString(R.string.status_running)
        is CaptureHealth.Starting -> getString(R.string.status_starting)
    }

    private fun startSession(projection: MediaProjection): Boolean {
        val record = buildAudioRecord(projection) ?: return false
        audioRecord = record
        modelBytes = loadModelAsset()

        // Selection lives entirely in the resolver; we just walk the chain.
        val resolver = EngineResolver(
            override = EnginePreferences.getOverride(this),
            modelAvailable = modelBytes != null,
        )
        val controller = FallbackController(resolver.chain(deviceFacts()))

        engine = AudioEngine()
        var plan: EnginePlan? = controller.current
        while (plan != null) {
            if (tryStart(plan)) {
                fallback = controller
                activeEngine = plan.id
                Log.i(TAG, "Capture engine started with ${plan.id}")
                return true
            }
            Log.w(TAG, "Engine ${plan.id} failed to start; trying next")
            plan = controller.advance()
        }
        failAndStop("No engine could be started")
        return false
    }

    private fun tryStart(plan: EnginePlan): Boolean {
        val rec = audioRecord ?: return false
        val eng = engine ?: return false
        if (eng.isRunning) eng.stop()
        return eng.start(
            rec,
            AudioFormatSpec.SAMPLE_RATE,
            AudioFormatSpec.CHANNEL_COUNT,
            plan.id.nativeId,
            plan.params,
            modelBytes,
        )
    }

    // Stops the current pipeline and restarts with `plan`. Reuses the live
    // MediaProjection/AudioRecord, so no new consent dialog. A brief audio gap
    // during the change is expected.
    private fun restartWith(plan: EnginePlan) {
        healthMonitor.reset()
        lastHealth = CaptureHealth.Starting
        if (tryStart(plan)) {
            activeEngine = plan.id
            updateNotification(statusTextFor(CaptureHealth.Starting))
        } else {
            val next = fallback?.advance()
            if (next != null) restartWith(next) else failAndStop("All engines failed")
        }
    }

    private fun deviceFacts(): DeviceFacts {
        val am = getSystemService(ActivityManager::class.java)
        val mem = ActivityManager.MemoryInfo().also { am.getMemoryInfo(it) }
        return DeviceFacts(
            cores = Runtime.getRuntime().availableProcessors(),
            sdkInt = Build.VERSION.SDK_INT,
            totalRamMb = mem.totalMem / (1024 * 1024),
            abiSupports64 = Build.SUPPORTED_64_BIT_ABIS.isNotEmpty(),
        )
    }

    private fun buildAudioRecord(projection: MediaProjection): AudioRecord? {
        val captureConfig = AudioPlaybackCaptureConfiguration.Builder(projection)
            .addMatchingUsage(AudioAttributesUsage.MEDIA)
            .addMatchingUsage(AudioAttributesUsage.GAME)
            .addMatchingUsage(AudioAttributesUsage.UNKNOWN)
            .build()

        val minBuffer = AudioRecord.getMinBufferSize(
            AudioFormatSpec.SAMPLE_RATE,
            AudioFormatSpec.CHANNEL_MASK,
            AudioFormatSpec.ENCODING,
        )
        val bufferBytes = AudioFormatSpec.recordBufferSizeBytes(minBuffer)

        val format = AudioFormat.Builder()
            .setEncoding(AudioFormatSpec.ENCODING)
            .setSampleRate(AudioFormatSpec.SAMPLE_RATE)
            .setChannelMask(AudioFormatSpec.CHANNEL_MASK)
            .build()

        val record = try {
            AudioRecord.Builder()
                .setAudioFormat(format)
                .setBufferSizeInBytes(bufferBytes)
                .setAudioPlaybackCaptureConfig(captureConfig)
                .build()
        } catch (e: UnsupportedOperationException) {
            failAndStop("AudioRecord unsupported: ${e.message}")
            return null
        } catch (e: SecurityException) {
            failAndStop("RECORD_AUDIO permission missing")
            return null
        }

        if (record.state != AudioRecord.STATE_INITIALIZED) {
            record.release()
            failAndStop("AudioRecord failed to initialize")
            return null
        }
        return record
    }

    /**
     * Loads the bundled ONNX separation model from assets, or returns null if
     * it is absent (the engine then runs passthrough). Kept small/simple: the
     * model is read fully into memory and handed to ONNX Runtime, which copies
     * it into the session.
     */
    private fun loadModelAsset(): ByteArray? = try {
        assets.open(MODEL_ASSET).use { it.readBytes() }
    } catch (e: java.io.FileNotFoundException) {
        Log.i(TAG, "No model asset ($MODEL_ASSET); running passthrough")
        null
    } catch (e: java.io.IOException) {
        Log.w(TAG, "Failed to read model asset", e)
        null
    }

    private fun stopCapture() {
        mainHandler.removeCallbacks(healthPoll)
        runCatching {
            getSystemService(AudioManager::class.java)
                .unregisterAudioDeviceCallback(deviceCallback)
        }
        engine?.release()
        engine = null
        // Native code already called AudioRecord.stop(); just release it.
        audioRecord?.release()
        audioRecord = null
        mediaProjection?.let {
            it.unregisterCallback(projectionCallback)
            it.stop()
        }
        mediaProjection = null
        fallback = null
        activeEngine = null
        modelBytes = null

        ServiceCompat.stopForeground(this, ServiceCompat.STOP_FOREGROUND_REMOVE)
        stopSelf()
    }

    private fun failAndStop(reason: String) {
        Log.e(TAG, "Capture failed: $reason")
        stopCapture()
    }

    private fun startForegroundCompat(notification: Notification) {
        ServiceCompat.startForeground(
            this,
            NOTIFICATION_ID,
            notification,
            ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PROJECTION,
        )
    }

    private fun createNotificationChannel() {
        val manager = getSystemService(NotificationManager::class.java)
        if (manager.getNotificationChannel(CHANNEL_ID) != null) return
        val channel = NotificationChannel(
            CHANNEL_ID,
            getString(R.string.notification_channel_name),
            NotificationManager.IMPORTANCE_LOW,
        ).apply {
            description = getString(R.string.notification_channel_desc)
            setShowBadge(false)
        }
        manager.createNotificationChannel(channel)
    }

    private fun buildNotification(statusText: String): Notification {
        val stopIntent = PendingIntent.getService(
            this,
            0,
            Intent(this, CaptureService::class.java).setAction(ACTION_STOP),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
        )
        val contentIntent = PendingIntent.getActivity(
            this,
            0,
            Intent(this, MainActivity::class.java)
                .addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
        )
        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle(getString(R.string.app_name))
            .setContentText(statusText)
            .setSmallIcon(R.drawable.ic_stat_capture)
            .setOngoing(true)
            .setContentIntent(contentIntent)
            .addAction(0, getString(R.string.action_stop), stopIntent)
            .setCategory(NotificationCompat.CATEGORY_SERVICE)
            .build()
    }

    private fun updateNotification(statusText: String) {
        getSystemService(NotificationManager::class.java)
            .notify(NOTIFICATION_ID, buildNotification(statusText))
    }

    /**
     * AudioAttributes.USAGE_* constants. Referenced by name to keep the capture
     * config readable; values mirror android.media.AudioAttributes.
     */
    private object AudioAttributesUsage {
        val MEDIA = android.media.AudioAttributes.USAGE_MEDIA
        val GAME = android.media.AudioAttributes.USAGE_GAME
        val UNKNOWN = android.media.AudioAttributes.USAGE_UNKNOWN
    }

    companion object {
        private const val TAG = "CaptureService"
        private const val CHANNEL_ID = "capture"
        private const val NOTIFICATION_ID = 1001
        private const val MODEL_ASSET = "bandscnet.onnx"

        private const val POLL_INTERVAL_MS = 1_000L

        const val ACTION_START = "com.codexsd.vocalremover.action.START"
        const val ACTION_STOP = "com.codexsd.vocalremover.action.STOP"
        const val EXTRA_RESULT_CODE = "result_code"
        const val EXTRA_RESULT_DATA = "result_data"

        fun startIntent(context: Context, resultCode: Int, resultData: Intent): Intent =
            Intent(context, CaptureService::class.java)
                .setAction(ACTION_START)
                .putExtra(EXTRA_RESULT_CODE, resultCode)
                .putExtra(EXTRA_RESULT_DATA, resultData)

        fun stopIntent(context: Context): Intent =
            Intent(context, CaptureService::class.java).setAction(ACTION_STOP)
    }
}
