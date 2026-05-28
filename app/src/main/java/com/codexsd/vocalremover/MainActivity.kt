package com.codexsd.vocalremover

import android.Manifest
import android.content.Context
import android.content.Intent
import android.media.projection.MediaProjectionManager
import android.os.Build
import android.os.Bundle
import android.widget.Button
import android.widget.TextView
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat

/**
 * Single-screen UI: one button that starts/stops the capture session, plus a
 * status line. Owns the permission and MediaProjection-consent dance, then
 * delegates the actual audio work to [CaptureService].
 *
 * UI state is tracked with [CaptureStateMachine]. Phase 1 transitions
 * optimistically (it assumes the service starts/stops successfully); a later
 * phase can replace that with real status callbacks from the service.
 */
class MainActivity : AppCompatActivity() {

    private lateinit var actionButton: Button
    private lateinit var statusText: TextView

    private var state: CaptureState = CaptureState.Idle

    private val projectionManager by lazy {
        getSystemService(Context.MEDIA_PROJECTION_SERVICE) as MediaProjectionManager
    }

    private val requestPermissions = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions(),
    ) { grants ->
        if (grants[Manifest.permission.RECORD_AUDIO] == true) {
            launchConsent()
        } else {
            dispatch(CaptureEvent.Failed(getString(R.string.error_no_record_permission)))
        }
    }

    private val requestConsent = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult(),
    ) { result ->
        val data = result.data
        if (result.resultCode == RESULT_OK && data != null) {
            dispatch(CaptureEvent.ConsentGranted)
            ContextCompat.startForegroundService(
                this,
                CaptureService.startIntent(this, result.resultCode, data),
            )
            // Optimistic: assume the service comes up. See class doc.
            dispatch(CaptureEvent.EngineStarted)
        } else {
            dispatch(CaptureEvent.ConsentDenied)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        actionButton = findViewById(R.id.action_button)
        statusText = findViewById(R.id.status_text)
        actionButton.setOnClickListener { onActionClicked() }
        findViewById<android.widget.Button>(R.id.settings_button).setOnClickListener {
            startActivity(Intent(this, SettingsActivity::class.java))
        }
        render()
    }

    private fun onActionClicked() {
        when (state) {
            is CaptureState.Idle, is CaptureState.Error -> dispatch(CaptureEvent.StartRequested)
            is CaptureState.Running -> {
                startService(CaptureService.stopIntent(this))
                dispatch(CaptureEvent.StopRequested)
                dispatch(CaptureEvent.EngineStopped)
            }
            else -> Unit // transitional states: button disabled
        }
    }

    private fun dispatch(event: CaptureEvent) {
        val next = CaptureStateMachine.reduce(state, event)
        state = next
        if (next is CaptureState.AwaitingConsent) {
            ensurePermissionsThenConsent()
        }
        render()
    }

    private fun ensurePermissionsThenConsent() {
        val needed = buildList {
            if (!hasPermission(Manifest.permission.RECORD_AUDIO)) {
                add(Manifest.permission.RECORD_AUDIO)
            }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU &&
                !hasPermission(Manifest.permission.POST_NOTIFICATIONS)
            ) {
                add(Manifest.permission.POST_NOTIFICATIONS)
            }
        }
        if (needed.isEmpty()) {
            launchConsent()
        } else {
            requestPermissions.launch(needed.toTypedArray())
        }
    }

    private fun launchConsent() {
        requestConsent.launch(projectionManager.createScreenCaptureIntent())
    }

    private fun hasPermission(permission: String): Boolean =
        ContextCompat.checkSelfPermission(this, permission) ==
            android.content.pm.PackageManager.PERMISSION_GRANTED

    private fun render() {
        when (val s = state) {
            is CaptureState.Idle -> {
                actionButton.isEnabled = true
                actionButton.setText(R.string.action_start)
                statusText.setText(R.string.status_idle)
            }
            is CaptureState.AwaitingConsent, is CaptureState.Starting -> {
                actionButton.isEnabled = false
                actionButton.setText(R.string.action_start)
                statusText.setText(R.string.status_starting)
            }
            is CaptureState.Running -> {
                actionButton.isEnabled = true
                actionButton.setText(R.string.action_stop)
                statusText.text = s.sourceLabel
                    ?.let { getString(R.string.status_running_source, it) }
                    ?: getString(R.string.status_running)
            }
            is CaptureState.Stopping -> {
                actionButton.isEnabled = false
                actionButton.setText(R.string.action_stop)
                statusText.setText(R.string.status_stopping)
            }
            is CaptureState.Error -> {
                actionButton.isEnabled = true
                actionButton.setText(R.string.action_start)
                statusText.text = getString(R.string.status_error, s.message)
            }
        }
    }
}
