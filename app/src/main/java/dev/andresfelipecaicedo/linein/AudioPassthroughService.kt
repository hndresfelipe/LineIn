package dev.andresfelipecaicedo.linein

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.media.AudioDeviceInfo
import android.media.AudioManager
import android.os.Binder
import android.os.IBinder
import android.util.Log
import androidx.core.app.NotificationCompat

class AudioPassthroughService : Service() {

    private val binder = LocalBinder()
    private var audioManager: AudioManager? = null
    private var isRunning = false

    inner class LocalBinder : Binder() {
        fun getService(): AudioPassthroughService = this@AudioPassthroughService
    }

    override fun onBind(intent: Intent?): IBinder = binder

    override fun onCreate() {
        super.onCreate()
        audioManager = getSystemService(Context.AUDIO_SERVICE) as AudioManager
        createNotificationChannel()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_START -> startPassthrough()
            ACTION_STOP -> stopPassthrough()
        }
        return START_STICKY
    }

    override fun onDestroy() {
        stopPassthrough()
        super.onDestroy()
    }

    fun startPassthrough() {
        if (isRunning) return

        // No audio focus requested: LineIn is a passthrough tool that must
        // coexist with other audio apps without interference from notifications

        // Find USB audio output device
        val usbOutputDevice = findUsbAudioOutputDevice()
        if (usbOutputDevice != null) {
            Log.i(TAG, "Found USB audio output device: ${usbOutputDevice.productName}, ID: ${usbOutputDevice.id}")
        } else {
            Log.w(TAG, "No USB audio output device found, using default")
        }

        // Create and start native engine
        PassthroughEngine.create()

        // Set output device before starting
        usbOutputDevice?.let {
            PassthroughEngine.setOutputDeviceId(it.id)
        }

        PassthroughEngine.setEffectOn(true)

        // Start foreground with notification
        startForeground(
            NOTIFICATION_ID,
            createNotification(),
            ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK or
                    ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE
        )

        isRunning = true
    }

    fun stopPassthrough() {
        if (!isRunning) return

        // Stop native engine
        PassthroughEngine.setEffectOn(false)
        PassthroughEngine.delete()

        // Stop foreground service
        stopForeground(STOP_FOREGROUND_REMOVE)
        stopSelf()

        isRunning = false
    }

    fun isPassthroughRunning(): Boolean = isRunning

    private fun findUsbAudioOutputDevice(): AudioDeviceInfo? {
        val devices = audioManager?.getDevices(AudioManager.GET_DEVICES_OUTPUTS) ?: return null

        // Log all output devices for debugging
        for (device in devices) {
            Log.d(TAG, "Output device: ${device.productName}, type: ${device.type}, id: ${device.id}")
        }

        // Find USB audio device
        return devices.firstOrNull { device ->
            device.type == AudioDeviceInfo.TYPE_USB_DEVICE ||
            device.type == AudioDeviceInfo.TYPE_USB_HEADSET ||
            device.type == AudioDeviceInfo.TYPE_USB_ACCESSORY
        }
    }

    private fun createNotificationChannel() {
        val channel = NotificationChannel(
            CHANNEL_ID,
            "LineIn Audio",
            NotificationManager.IMPORTANCE_LOW
        ).apply {
            description = "Audio passthrough is active"
            setShowBadge(false)
        }

        val notificationManager = getSystemService(NotificationManager::class.java)
        notificationManager.createNotificationChannel(channel)
    }

    private fun createNotification(): Notification {
        val pendingIntent = PendingIntent.getActivity(
            this,
            0,
            Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT
        )

        val stopIntent = PendingIntent.getService(
            this,
            0,
            Intent(this, AudioPassthroughService::class.java).apply {
                action = ACTION_STOP
            },
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT
        )

        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("LineIn")
            .setContentText("Audio passthrough is active")
            .setSmallIcon(R.drawable.ic_notification)
            .setContentIntent(pendingIntent)
            .addAction(R.drawable.ic_notification, "Stop", stopIntent)
            .setOngoing(true)
            .build()
    }

    companion object {
        private const val TAG = "AudioPassthroughService"
        const val CHANNEL_ID = "linein_passthrough_channel"
        const val NOTIFICATION_ID = 1
        const val ACTION_START = "dev.andresfelipecaicedo.linein.START"
        const val ACTION_STOP = "dev.andresfelipecaicedo.linein.STOP"

        fun startService(context: Context) {
            val intent = Intent(context, AudioPassthroughService::class.java).apply {
                action = ACTION_START
            }
            context.startForegroundService(intent)
        }

        fun stopService(context: Context) {
            val intent = Intent(context, AudioPassthroughService::class.java).apply {
                action = ACTION_STOP
            }
            context.startService(intent)
        }
    }
}
