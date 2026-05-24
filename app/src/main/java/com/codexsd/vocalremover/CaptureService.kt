package com.codexsd.vocalremover

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
import android.media.projection.MediaProjection
import android.media.projection.MediaProjectionManager
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.util.Log
import androidx.core.app.NotificationCompat
import androidx.core.app.ServiceCompat
import com.codexsd.vocalremover.audio.AudioEngine
import com.codexsd.vocalremover.audio.AudioFormatSpec

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

    private val projectionCallback = object : MediaProjection.Callback() {
        override fun onStop() {
            Log.i(TAG, "MediaProjection stopped by system/user")
            stopCapture()
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

        if (!startEngine(projection)) {
            return
        }

        updateNotification(getString(R.string.status_running))
    }

    private fun startEngine(projection: MediaProjection): Boolean {
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
            return false
        } catch (e: SecurityException) {
            failAndStop("RECORD_AUDIO permission missing")
            return false
        }

        if (record.state != AudioRecord.STATE_INITIALIZED) {
            record.release()
            failAndStop("AudioRecord failed to initialize")
            return false
        }
        audioRecord = record

        val audioEngine = AudioEngine()
        engine = audioEngine
        val started = audioEngine.start(
            record,
            AudioFormatSpec.SAMPLE_RATE,
            AudioFormatSpec.CHANNEL_COUNT,
        )
        if (!started) {
            failAndStop("Native engine failed to start")
            return false
        }
        Log.i(TAG, "Capture engine started")
        return true
    }

    private fun stopCapture() {
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
