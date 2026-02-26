package dev.andresfelipecaicedo.linein

object PassthroughEngine {
    init {
        System.loadLibrary("linein")
    }

    external fun nativeCreate(): Boolean
    external fun nativeDelete()
    external fun nativeSetEffectOn(isOn: Boolean)
    external fun nativeSetGain(gain: Float)
    external fun nativeSetOutputDeviceId(deviceId: Int)
    external fun nativeIsInputMMAP(): Boolean
    external fun nativeIsOutputMMAP(): Boolean
    external fun nativeGetInputLatencyMs(): Int
    external fun nativeGetOutputLatencyMs(): Int
    external fun nativeSetTargetBufferMs(ms: Int)
    external fun nativeSetDrainRate(rate: Float)
    external fun nativeGetCurrentBufferMs(): Int

    fun create(): Boolean = nativeCreate()

    fun delete() = nativeDelete()

    fun setEffectOn(isOn: Boolean) = nativeSetEffectOn(isOn)

    fun setGain(gain: Float) = nativeSetGain(gain)

    fun setOutputDeviceId(deviceId: Int) = nativeSetOutputDeviceId(deviceId)

    fun isInputMMAP(): Boolean = nativeIsInputMMAP()

    fun isOutputMMAP(): Boolean = nativeIsOutputMMAP()

    fun getInputLatencyMs(): Int = nativeGetInputLatencyMs()

    fun getOutputLatencyMs(): Int = nativeGetOutputLatencyMs()

    fun setTargetBufferMs(ms: Int) = nativeSetTargetBufferMs(ms)

    fun setDrainRate(rate: Float) = nativeSetDrainRate(rate)

    fun getCurrentBufferMs(): Int = nativeGetCurrentBufferMs()
}
