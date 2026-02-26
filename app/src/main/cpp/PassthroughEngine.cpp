#include "PassthroughEngine.h"
#include <android/log.h>
#include <thread>

#define LOG_TAG "PassthroughEngine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

PassthroughEngine::PassthroughEngine() {
    LOGI("PassthroughEngine created");
}

PassthroughEngine::~PassthroughEngine() {
    closeStreams();
    LOGI("PassthroughEngine destroyed");
}

void PassthroughEngine::setEffectOn(bool isOn) {
    if (isOn == mIsEffectOn) {
        return;
    }

    if (isOn) {
        if (openStreams()) {
            mIsEffectOn = true;
            LOGI("Audio passthrough started");
        } else {
            LOGE("Failed to open audio streams");
        }
    } else {
        closeStreams();
        mIsEffectOn = false;
        LOGI("Audio passthrough stopped");
    }
}

bool PassthroughEngine::openStreams() {
    // Create the full-duplex callback first (needed for output stream builder)
    mFullDuplexPass = std::make_unique<FullDuplexPass>();

    // Create output stream with callback set on builder (stereo output)
    // Try Exclusive mode for potentially lower latency
    oboe::AudioStreamBuilder outputBuilder;
    outputBuilder.setDirection(oboe::Direction::Output)
            ->setFormat(oboe::AudioFormat::Float)
            ->setSharingMode(oboe::SharingMode::Exclusive)
            ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
            ->setChannelCount(mOutputChannelCount)
            ->setDataCallback(mFullDuplexPass.get())
            ->setErrorCallback(this);

    // Route to specific USB device if set
    if (mOutputDeviceId != oboe::kUnspecified) {
        outputBuilder.setDeviceId(mOutputDeviceId);
        LOGI("Requesting output device ID: %d", mOutputDeviceId);
    }

    oboe::Result result = outputBuilder.openStream(mOutputStream);
    if (result != oboe::Result::OK) {
        LOGE("Failed to open output stream: %s", oboe::convertToText(result));
        mFullDuplexPass.reset();
        return false;
    }

    // Get the actual sample rate from output stream
    mSampleRate = mOutputStream->getSampleRate();

    // Detect MMAP: AAudio with small burst duration indicates MMAP, large burst = Legacy AudioTrack
    float outputBurstMs = (mOutputStream->getFramesPerBurst() * 1000.0f) / mSampleRate;
    mOutputUsesMMAP = (mOutputStream->getAudioApi() == oboe::AudioApi::AAudio && outputBurstMs < 5.0f);

    // Set buffer size based on mode:
    // - MMAP: 1x burst for minimum latency
    // - Legacy: 2x burst - balance between latency and stability
    int32_t outputBufferMultiplier = mOutputUsesMMAP ? 1 : 2;
    mOutputStream->setBufferSizeInFrames(mOutputStream->getFramesPerBurst() * outputBufferMultiplier);

    LOGI("Output stream opened: sampleRate=%d, channelCount=%d, framesPerBurst=%d, bufferSize=%d, API=%s, MMAP=%s",
         mSampleRate,
         mOutputStream->getChannelCount(),
         mOutputStream->getFramesPerBurst(),
         mOutputStream->getBufferSizeInFrames(),
         oboe::convertToText(mOutputStream->getAudioApi()),
         mOutputUsesMMAP ? "YES" : "NO");

    // Create input stream with matching sample rate (mono input for iRig HD 2)
    // No callback - we read synchronously from the output callback
    // Use VoicePerformance preset for lowest latency real-time input
    oboe::AudioStreamBuilder inputBuilder;
    inputBuilder.setDirection(oboe::Direction::Input)
            ->setFormat(oboe::AudioFormat::Float)
            ->setSharingMode(oboe::SharingMode::Exclusive)
            ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
            ->setChannelCount(mInputChannelCount)
            ->setSampleRate(mSampleRate)
            ->setInputPreset(oboe::InputPreset::VoicePerformance)
            ->setErrorCallback(this);

    result = inputBuilder.openStream(mInputStream);
    if (result != oboe::Result::OK) {
        LOGE("Failed to open input stream: %s", oboe::convertToText(result));
        mOutputStream->close();
        mOutputStream.reset();
        mFullDuplexPass.reset();
        return false;
    }

    // Set buffer size to 1x burst for minimum latency
    mInputStream->setBufferSizeInFrames(mInputStream->getFramesPerBurst());

    // Detect MMAP for input
    float inputBurstMs = (mInputStream->getFramesPerBurst() * 1000.0f) / mSampleRate;
    mInputUsesMMAP = (mInputStream->getAudioApi() == oboe::AudioApi::AAudio && inputBurstMs < 5.0f);

    LOGI("Input stream opened: sampleRate=%d, channelCount=%d, framesPerBurst=%d, bufferSize=%d, API=%s, MMAP=%s",
         mInputStream->getSampleRate(),
         mInputStream->getChannelCount(),
         mInputStream->getFramesPerBurst(),
         mInputStream->getBufferSizeInFrames(),
         oboe::convertToText(mInputStream->getAudioApi()),
         mInputUsesMMAP ? "YES" : "NO");

    // Set streams on the full-duplex callback
    mFullDuplexPass->setInputStream(mInputStream.get());
    mFullDuplexPass->setOutputStream(mOutputStream.get());

    // Start both streams using FullDuplexStream's coordinated start
    result = mFullDuplexPass->start();
    if (result != oboe::Result::OK) {
        LOGE("Failed to start full-duplex streams: %s", oboe::convertToText(result));
        closeStreams();
        return false;
    }

    // Log latency information
    auto inputLatency = mInputStream->calculateLatencyMillis();
    auto outputLatency = mOutputStream->calculateLatencyMillis();

    // Calculate buffer-based latency as fallback
    int32_t inputBufferLatencyMs = (mInputStream->getBufferSizeInFrames() * 1000) / mSampleRate;
    int32_t outputBufferLatencyMs = (mOutputStream->getBufferSizeInFrames() * 1000) / mSampleRate;

    LOGI("Both streams started successfully");
    LOGI("Latency - Input: %s (buffer: %dms), Output: %s (buffer: %dms)",
         inputLatency ? (std::to_string(static_cast<int>(inputLatency.value())) + "ms").c_str() : "unknown",
         inputBufferLatencyMs,
         outputLatency ? (std::to_string(static_cast<int>(outputLatency.value())) + "ms").c_str() : "unknown",
         outputBufferLatencyMs);
    LOGI("Estimated round-trip buffer latency: %dms (actual may be higher with Legacy mode)",
         inputBufferLatencyMs + outputBufferLatencyMs);

    return true;
}

void PassthroughEngine::closeStreams() {
    // Stop using FullDuplexStream's coordinated stop
    if (mFullDuplexPass) {
        mFullDuplexPass->stop();
    }

    if (mInputStream) {
        mInputStream->close();
        mInputStream.reset();
    }

    if (mOutputStream) {
        mOutputStream->close();
        mOutputStream.reset();
    }

    mFullDuplexPass.reset();
    LOGI("Streams closed");
}

void PassthroughEngine::setGain(float gain) {
    if (mFullDuplexPass) {
        mFullDuplexPass->setGain(gain);
        LOGI("Gain set to %.2f", gain);
    }
}

void PassthroughEngine::setOutputDeviceId(int32_t deviceId) {
    mOutputDeviceId = deviceId;
    LOGI("Output device ID set to %d", deviceId);
}

bool PassthroughEngine::isInputMMAP() const {
    return mInputUsesMMAP;
}

bool PassthroughEngine::isOutputMMAP() const {
    return mOutputUsesMMAP;
}

int32_t PassthroughEngine::getInputLatencyMs() const {
    if (mInputStream) {
        // Try to get actual latency from stream
        auto result = mInputStream->calculateLatencyMillis();
        if (result) {
            return static_cast<int32_t>(result.value());
        }
        // Fallback to buffer-based estimate
        int32_t frames = mInputStream->getBufferSizeInFrames();
        int32_t sampleRate = mInputStream->getSampleRate();
        if (sampleRate > 0) {
            int32_t bufferLatency = (frames * 1000) / sampleRate;
            // Legacy mode adds additional internal buffering
            if (!mInputUsesMMAP) {
                bufferLatency *= 2;  // Conservative estimate for Legacy overhead
            }
            return bufferLatency;
        }
    }
    return -1;
}

int32_t PassthroughEngine::getOutputLatencyMs() const {
    if (mOutputStream) {
        // Try to get actual latency from stream
        auto result = mOutputStream->calculateLatencyMillis();
        if (result) {
            return static_cast<int32_t>(result.value());
        }
        // Fallback to buffer-based estimate
        int32_t frames = mOutputStream->getBufferSizeInFrames();
        int32_t sampleRate = mOutputStream->getSampleRate();
        if (sampleRate > 0) {
            int32_t bufferLatency = (frames * 1000) / sampleRate;
            // Legacy mode (AudioTrack) adds significant internal buffering
            // Typically 2-4x the buffer size for mixing and conversion
            if (!mOutputUsesMMAP) {
                bufferLatency *= 3;  // Conservative estimate for Legacy overhead
            }
            return bufferLatency;
        }
    }
    return -1;
}

void PassthroughEngine::restartStreams() {
    std::lock_guard<std::mutex> lock(mRestartMutex);
    if (mIsEffectOn) {
        closeStreams();
        openStreams();
    }
}

void PassthroughEngine::onErrorBeforeClose(oboe::AudioStream *stream, oboe::Result result) {
    LOGE("Stream error before close: %s", oboe::convertToText(result));
}

void PassthroughEngine::onErrorAfterClose(oboe::AudioStream *stream, oboe::Result result) {
    LOGE("Stream error after close: %s, restarting...", oboe::convertToText(result));
    if (result == oboe::Result::ErrorDisconnected) {
        // Restart on a separate thread to avoid deadlock
        // Use weak_ptr to prevent use-after-free if engine is destroyed
        std::weak_ptr<PassthroughEngine> weakSelf = shared_from_this();
        std::thread([weakSelf]() {
            if (auto self = weakSelf.lock()) {
                self->restartStreams();
            }
        }).detach();
    }
}

void PassthroughEngine::setTargetBufferMs(int32_t ms) {
    if (mFullDuplexPass && mSampleRate > 0) {
        int32_t frames = (ms * mSampleRate) / 1000;
        mFullDuplexPass->setTargetBufferFrames(frames);
        LOGI("Target buffer set to %dms (%d frames)", ms, frames);
    }
}

void PassthroughEngine::setDrainRate(float rate) {
    if (mFullDuplexPass) {
        mFullDuplexPass->setDrainRate(rate);
        LOGI("Drain rate set to %.2f", rate);
    }
}

int32_t PassthroughEngine::getCurrentBufferMs() const {
    if (mFullDuplexPass && mSampleRate > 0) {
        int32_t frames = mFullDuplexPass->getCurrentBufferFrames();
        return (frames * 1000) / mSampleRate;
    }
    return -1;
}
