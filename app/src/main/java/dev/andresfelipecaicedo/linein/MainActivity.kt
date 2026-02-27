package dev.andresfelipecaicedo.linein

import android.Manifest
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.content.pm.PackageManager
import android.os.Bundle
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat
import dev.andresfelipecaicedo.linein.ui.theme.LineInTheme

data class AudioStatus(
    val inputMMAP: Boolean = false,
    val outputMMAP: Boolean = false,
    val inputLatencyMs: Int = -1,
    val outputLatencyMs: Int = -1,
    val currentBufferMs: Int = -1
)

class MainActivity : ComponentActivity() {

    private var isPassthroughActive by mutableStateOf(false)
    private var permissionsGranted by mutableStateOf(false)
    private var gain by mutableFloatStateOf(8.0f)
    private var targetBufferMs by mutableStateOf(0)  // 0 = disabled
    private var drainRate by mutableFloatStateOf(0.0f)  // 0 = disabled
    private var audioStatus by mutableStateOf(AudioStatus())
    private var service: AudioPassthroughService? = null
    private var isBound = false
    private val handler = Handler(Looper.getMainLooper())

    private val serviceConnection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName?, binder: IBinder?) {
            val localBinder = binder as AudioPassthroughService.LocalBinder
            service = localBinder.getService()
            isBound = true
            isPassthroughActive = service?.isPassthroughRunning() ?: false
            if (isPassthroughActive) {
                updateAudioStatus()
            }
        }

        override fun onServiceDisconnected(name: ComponentName?) {
            service = null
            isBound = false
        }
    }

    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { permissions ->
        permissionsGranted = permissions.values.all { it }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()

        checkAndRequestPermissions()

        setContent {
            LineInTheme {
                Scaffold(modifier = Modifier.fillMaxSize()) { innerPadding ->
                    MainScreen(
                        isActive = isPassthroughActive,
                        hasPermissions = permissionsGranted,
                        gain = gain,
                        targetBufferMs = targetBufferMs,
                        drainRate = drainRate,
                        audioStatus = audioStatus,
                        onGainChange = { newGain ->
                            gain = newGain
                            if (isPassthroughActive) {
                                PassthroughEngine.setGain(newGain)
                            }
                        },
                        onTargetBufferChange = { newTarget ->
                            targetBufferMs = newTarget
                            PassthroughEngine.setTargetBufferMs(newTarget)
                        },
                        onDrainRateChange = { newRate ->
                            drainRate = newRate
                            PassthroughEngine.setDrainRate(newRate)
                        },
                        onToggle = { togglePassthrough() },
                        onRequestPermissions = { checkAndRequestPermissions() },
                        modifier = Modifier.padding(innerPadding)
                    )
                }
            }
        }
    }

    override fun onStart() {
        super.onStart()
        Intent(this, AudioPassthroughService::class.java).also { intent ->
            bindService(intent, serviceConnection, Context.BIND_AUTO_CREATE)
        }
    }

    override fun onStop() {
        handler.removeCallbacksAndMessages(null)
        super.onStop()
        if (isBound) {
            unbindService(serviceConnection)
            isBound = false
        }
    }

    private fun checkAndRequestPermissions() {
        val requiredPermissions = arrayOf(
            Manifest.permission.RECORD_AUDIO,
            Manifest.permission.POST_NOTIFICATIONS
        )

        val notGranted = requiredPermissions.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }

        if (notGranted.isEmpty()) {
            permissionsGranted = true
        } else {
            permissionLauncher.launch(notGranted.toTypedArray())
        }
    }

    private fun updateAudioStatus() {
        // Delay slightly to ensure streams are fully initialized
        handler.postDelayed({
            audioStatus = AudioStatus(
                inputMMAP = PassthroughEngine.isInputMMAP(),
                outputMMAP = PassthroughEngine.isOutputMMAP(),
                inputLatencyMs = PassthroughEngine.getInputLatencyMs(),
                outputLatencyMs = PassthroughEngine.getOutputLatencyMs(),
                currentBufferMs = PassthroughEngine.getCurrentBufferMs()
            )
            // Keep updating while active
            if (isPassthroughActive) {
                updateAudioStatus()
            }
        }, 500)
    }

    private fun togglePassthrough() {
        if (!permissionsGranted) {
            checkAndRequestPermissions()
            return
        }

        if (isPassthroughActive) {
            AudioPassthroughService.stopService(this)
            isPassthroughActive = false
            audioStatus = AudioStatus()
        } else {
            AudioPassthroughService.startService(this)
            isPassthroughActive = true
            // Apply current gain setting
            PassthroughEngine.setGain(gain)
            // Update audio status after a short delay
            updateAudioStatus()
        }
    }
}

@Composable
fun MainScreen(
    isActive: Boolean,
    hasPermissions: Boolean,
    gain: Float,
    targetBufferMs: Int,
    drainRate: Float,
    audioStatus: AudioStatus,
    onGainChange: (Float) -> Unit,
    onTargetBufferChange: (Int) -> Unit,
    onDrainRateChange: (Float) -> Unit,
    onToggle: () -> Unit,
    onRequestPermissions: () -> Unit,
    modifier: Modifier = Modifier
) {
    Column(
        modifier = modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(vertical = 32.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Top
    ) {
        Text(
            text = "LineIn",
            style = MaterialTheme.typography.headlineLarge
        )

        Spacer(modifier = Modifier.height(16.dp))

        Text(
            text = if (isActive) "Audio passthrough is active" else "Audio passthrough is stopped",
            style = MaterialTheme.typography.bodyLarge,
            color = if (isActive) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurfaceVariant
        )

        Spacer(modifier = Modifier.height(32.dp))

        // Audio Status Display
        if (isActive) {
            AudioStatusCard(audioStatus)
            Spacer(modifier = Modifier.height(32.dp))
        }

        if (!hasPermissions) {
            Button(
                onClick = onRequestPermissions,
                modifier = Modifier.size(width = 200.dp, height = 56.dp)
            ) {
                Text("Grant Permissions")
            }
        } else {
            Button(
                onClick = onToggle,
                modifier = Modifier.size(width = 200.dp, height = 56.dp),
                colors = if (isActive) {
                    ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.error)
                } else {
                    ButtonDefaults.buttonColors()
                }
            ) {
                Text(if (isActive) "Stop" else "Start")
            }

            Spacer(modifier = Modifier.height(32.dp))

            // Volume control
            Text(
                text = "Volume",
                style = MaterialTheme.typography.titleMedium
            )

            Spacer(modifier = Modifier.height(8.dp))

            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 32.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Slider(
                    value = gain,
                    onValueChange = onGainChange,
                    valueRange = 3.0f..12.0f,
                    modifier = Modifier.weight(1f)
                )
                Spacer(modifier = Modifier.width(16.dp))
                Text(
                    text = String.format("%.1fx", gain),
                    style = MaterialTheme.typography.bodyLarge,
                    modifier = Modifier.width(48.dp)
                )
            }

            // Latency Tuning Section
            if (isActive) {
                Spacer(modifier = Modifier.height(24.dp))
                LatencyTuningCard(
                    targetBufferMs = targetBufferMs,
                    drainRate = drainRate,
                    currentBufferMs = audioStatus.currentBufferMs,
                    onTargetBufferChange = onTargetBufferChange,
                    onDrainRateChange = onDrainRateChange
                )
            }
        }
    }
}

@Composable
fun LatencyTuningCard(
    targetBufferMs: Int,
    drainRate: Float,
    currentBufferMs: Int,
    onTargetBufferChange: (Int) -> Unit,
    onDrainRateChange: (Float) -> Unit
) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 32.dp)
            .clip(RoundedCornerShape(12.dp))
            .background(MaterialTheme.colorScheme.surfaceVariant)
            .padding(16.dp)
    ) {
        Text(
            text = "Latency Tuning",
            style = MaterialTheme.typography.titleMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant
        )

        Spacer(modifier = Modifier.height(8.dp))

        // Current buffer display
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            Text(
                text = "Current Buffer:",
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
            Text(
                text = if (currentBufferMs > 0) "${currentBufferMs}ms" else "---",
                style = MaterialTheme.typography.bodyMedium,
                color = if (currentBufferMs > 50) MaterialTheme.colorScheme.error
                       else MaterialTheme.colorScheme.primary
            )
        }

        Spacer(modifier = Modifier.height(16.dp))

        // Target Buffer control
        Text(
            text = "Target Buffer",
            style = MaterialTheme.typography.labelLarge,
            color = MaterialTheme.colorScheme.onSurfaceVariant
        )
        Text(
            text = "Lower = less latency, but may cause audio glitches",
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.7f)
        )

        Spacer(modifier = Modifier.height(4.dp))

        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Slider(
                value = targetBufferMs.toFloat(),
                onValueChange = { onTargetBufferChange(it.toInt()) },
                valueRange = 0f..100f,
                steps = 9,
                modifier = Modifier.weight(1f)
            )
            Spacer(modifier = Modifier.width(8.dp))
            Text(
                text = if (targetBufferMs == 0) "OFF" else "${targetBufferMs}ms",
                style = MaterialTheme.typography.bodyMedium,
                modifier = Modifier.width(48.dp)
            )
        }

        Spacer(modifier = Modifier.height(12.dp))

        // Drain Rate control
        Text(
            text = "Drain Speed",
            style = MaterialTheme.typography.labelLarge,
            color = MaterialTheme.colorScheme.onSurfaceVariant
        )
        Text(
            text = "How fast to reduce buffer when over target (0=off, 1=fast)",
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.7f)
        )

        Spacer(modifier = Modifier.height(4.dp))

        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Slider(
                value = drainRate,
                onValueChange = onDrainRateChange,
                valueRange = 0f..1f,
                modifier = Modifier.weight(1f)
            )
            Spacer(modifier = Modifier.width(8.dp))
            Text(
                text = if (drainRate == 0f) "OFF" else String.format("%.1f", drainRate),
                style = MaterialTheme.typography.bodyMedium,
                modifier = Modifier.width(48.dp)
            )
        }

        Spacer(modifier = Modifier.height(16.dp))

        // Presets
        Text(
            text = "Presets",
            style = MaterialTheme.typography.labelLarge,
            color = MaterialTheme.colorScheme.onSurfaceVariant
        )

        Spacer(modifier = Modifier.height(8.dp))

        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            // Off preset
            OutlinedButton(
                onClick = {
                    onTargetBufferChange(0)
                    onDrainRateChange(0f)
                },
                modifier = Modifier.weight(1f)
            ) {
                Text("Off", style = MaterialTheme.typography.labelMedium)
            }

            // Low Latency preset (recommended)
            Button(
                onClick = {
                    onTargetBufferChange(19)
                    onDrainRateChange(1.0f)
                },
                modifier = Modifier.weight(1f)
            ) {
                Text("Low", style = MaterialTheme.typography.labelMedium)
            }

            // Safe preset
            OutlinedButton(
                onClick = {
                    onTargetBufferChange(50)
                    onDrainRateChange(0.5f)
                },
                modifier = Modifier.weight(1f)
            ) {
                Text("Safe", style = MaterialTheme.typography.labelMedium)
            }
        }

        Spacer(modifier = Modifier.height(8.dp))

        // Preset descriptions
        Text(
            text = "Low: ~19ms buffer, best response (recommended)",
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.primary
        )
    }
}

@Composable
fun AudioStatusCard(status: AudioStatus) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 32.dp)
            .clip(RoundedCornerShape(12.dp))
            .background(MaterialTheme.colorScheme.surfaceVariant)
            .padding(16.dp)
    ) {
        Text(
            text = "Audio Status",
            style = MaterialTheme.typography.titleMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant
        )

        Spacer(modifier = Modifier.height(12.dp))

        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            Column {
                Text(
                    text = "Input",
                    style = MaterialTheme.typography.labelMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
                Spacer(modifier = Modifier.height(4.dp))
                StatusBadge(
                    isLowLatency = status.inputMMAP,
                    latencyMs = status.inputLatencyMs
                )
            }

            Column(horizontalAlignment = Alignment.End) {
                Text(
                    text = "Output",
                    style = MaterialTheme.typography.labelMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
                Spacer(modifier = Modifier.height(4.dp))
                StatusBadge(
                    isLowLatency = status.outputMMAP,
                    latencyMs = status.outputLatencyMs
                )
            }
        }

        if (!status.inputMMAP || !status.outputMMAP) {
            Spacer(modifier = Modifier.height(12.dp))
            Text(
                text = if (!status.outputMMAP) "Output uses Legacy mode (~40ms latency)"
                       else "Input uses Legacy mode",
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.error
            )
        }
    }
}

@Composable
fun StatusBadge(isLowLatency: Boolean, latencyMs: Int) {
    val backgroundColor = if (isLowLatency) Color(0xFF4CAF50) else Color(0xFFFF9800)
    val text = if (isLowLatency) "MMAP" else "Legacy"
    val latencyText = if (latencyMs > 0) " ~${latencyMs}ms" else ""

    Text(
        text = "$text$latencyText",
        style = MaterialTheme.typography.labelLarge,
        color = Color.White,
        modifier = Modifier
            .clip(RoundedCornerShape(4.dp))
            .background(backgroundColor)
            .padding(horizontal = 8.dp, vertical = 4.dp)
    )
}
