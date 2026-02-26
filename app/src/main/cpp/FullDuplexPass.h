#ifndef GUITARPASSTHROUGH_FULLDUPLEXPASS_H
#define GUITARPASSTHROUGH_FULLDUPLEXPASS_H

#include <oboe/Oboe.h>
#include <android/log.h>
#include <vector>
#include <chrono>
#include <thread>
#include <cmath>

#define FDP_LOG_TAG "FullDuplexPass"
#define FDP_LOGI(...) __android_log_print(ANDROID_LOG_INFO, FDP_LOG_TAG, __VA_ARGS__)
#define FDP_LOGW(...) __android_log_print(ANDROID_LOG_WARN, FDP_LOG_TAG, __VA_ARGS__)

// Direct passthrough callback - bypasses FullDuplexStream's internal buffering
class FullDuplexPass : public oboe::AudioStreamDataCallback {
public:
    FullDuplexPass() = default;

    void setInputStream(oboe::AudioStream *stream) { mInputStream = stream; }
    void setOutputStream(oboe::AudioStream *stream) { mOutputStream = stream; }
    oboe::AudioStream* getInputStream() const { return mInputStream; }
    oboe::AudioStream* getOutputStream() const { return mOutputStream; }

    void setGain(float gain) { mGain = gain; }
    float getGain() const { return mGain; }

    // Latency tuning parameters
    // Target buffer: how many frames we want to maintain in input buffer (lower = less latency, more risk)
    void setTargetBufferFrames(int32_t frames) { mTargetBufferFrames = frames; }
    int32_t getTargetBufferFrames() const { return mTargetBufferFrames; }

    // Drain rate: how many extra frames to read per callback when over target (higher = faster drain, more artifacts)
    // 0 = no draining (current behavior), 1.0 = read double frames, 0.5 = read 50% extra
    void setDrainRate(float rate) { mDrainRate = rate; }
    float getDrainRate() const { return mDrainRate; }

    // Get current buffer level for UI display
    int32_t getCurrentBufferFrames() const { return mLastAvailableFrames; }

    oboe::Result start() {
        mCallbackCount = 0;
        mTotalFramesRead = 0;
        mTotalFramesWritten = 0;
        mFramesDrained = 0;
        mInputXRunCount = 0;
        mOutputXRunCount = 0;

        if (mInputStream) {
            auto result = mInputStream->requestStart();
            if (result != oboe::Result::OK) return result;

            // Wait for input stream to buffer some samples before starting output
            // This helps ensure data is ready for the first output callback
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        if (mOutputStream) {
            return mOutputStream->requestStart();
        }
        return oboe::Result::ErrorNull;
    }

    oboe::Result stop() {
        // Log statistics
        FDP_LOGI("Session stats: callbacks=%d, framesRead=%lld, framesWritten=%lld, framesDrained=%lld, inputXRuns=%d, outputXRuns=%d",
                 mCallbackCount, (long long)mTotalFramesRead, (long long)mTotalFramesWritten,
                 (long long)mFramesDrained, mInputXRunCount, mOutputXRunCount);

        oboe::Result result = oboe::Result::OK;
        if (mInputStream) {
            result = mInputStream->requestStop();
        }
        if (mOutputStream) {
            auto outResult = mOutputStream->requestStop();
            if (result == oboe::Result::OK) result = outResult;
        }
        return result;
    }

    oboe::DataCallbackResult onAudioReady(
            oboe::AudioStream *outputStream,
            void *audioData,
            int32_t numFrames) override {

        mCallbackCount++;
        float *outputFloats = static_cast<float *>(audioData);
        int32_t outputChannelCount = outputStream->getChannelCount();

        // Check for XRuns (buffer underruns/overruns)
        if (mInputStream) {
            auto inputXRunResult = mInputStream->getXRunCount();
            if (inputXRunResult && inputXRunResult.value() > mInputXRunCount) {
                FDP_LOGW("Input XRun detected! Total: %d", inputXRunResult.value());
                mInputXRunCount = inputXRunResult.value();
            }
        }
        auto outputXRunResult = outputStream->getXRunCount();
        if (outputXRunResult && outputXRunResult.value() > mOutputXRunCount) {
            FDP_LOGW("Output XRun detected! Total: %d", outputXRunResult.value());
            mOutputXRunCount = outputXRunResult.value();
        }

        if (!mInputStream) {
            // No input, fill with silence
            memset(outputFloats, 0, numFrames * outputChannelCount * sizeof(float));
            return oboe::DataCallbackResult::Continue;
        }

        int32_t inputChannelCount = mInputStream->getChannelCount();

        // Check how many frames are available
        auto availResult = mInputStream->getAvailableFrames();
        int32_t availableFrames = availResult ? availResult.value() : 0;
        mLastAvailableFrames = availableFrames;

        // Calculate how many frames to read
        // Base: numFrames (what output needs)
        // Extra: if buffer is over target and draining is enabled, read more to gradually reduce
        int32_t framesToRead = numFrames;
        int32_t framesDrained = 0;

        if (mDrainRate > 0.0f && mTargetBufferFrames > 0) {
            int32_t excessFrames = availableFrames - mTargetBufferFrames;
            if (excessFrames > 0) {
                // Gradually drain: read extra frames based on drain rate
                // drainRate 0.5 = read 50% extra, 1.0 = read double
                int32_t extraFrames = static_cast<int32_t>(numFrames * mDrainRate);
                // Don't drain more than the excess
                extraFrames = std::min(extraFrames, excessFrames);
                framesToRead = numFrames + extraFrames;
                framesDrained = extraFrames;
            }
        }

        // Ensure buffer is large enough
        int32_t inputSamplesNeeded = framesToRead * inputChannelCount;
        if (mInputBuffer.size() < static_cast<size_t>(inputSamplesNeeded)) {
            mInputBuffer.resize(inputSamplesNeeded);
        }

        // Read frames (including any extra for draining)
        auto readResult = mInputStream->read(mInputBuffer.data(), framesToRead, 0);

        int32_t framesRead = 0;
        if (readResult.value() > 0) {
            framesRead = readResult.value();
            mTotalFramesRead += framesRead;
            if (framesDrained > 0) {
                mFramesDrained += std::min(framesDrained, framesRead - numFrames);
            }
        }

        // Calculate which frames to use for output
        // If we read more than needed, use the NEWEST frames (skip oldest)
        int32_t framesToSkip = 0;
        int32_t framesToUse = framesRead;
        if (framesRead > numFrames) {
            framesToSkip = framesRead - numFrames;
            framesToUse = numFrames;
        }

        // Log periodically (every ~1 second at 48kHz with 240 frame bursts)
        if (mCallbackCount % 200 == 0) {
            int32_t bufferLatencyMs = (availableFrames * 1000) / mInputStream->getSampleRate();
            FDP_LOGI("Callback #%d: avail=%d (%dms), read=%d, skip=%d, target=%d, drain=%.1f",
                     mCallbackCount, availableFrames, bufferLatencyMs, framesRead,
                     framesToSkip, mTargetBufferFrames, mDrainRate);
        }

        mTotalFramesWritten += numFrames;

        // Process audio: mono to stereo with gain and soft limiting
        // When draining, skip oldest frames and use newest (framesToSkip offset)
        if (inputChannelCount == 1 && outputChannelCount == 2) {
            for (int i = 0; i < framesToUse; i++) {
                // Use frames starting at framesToSkip (newest frames)
                float sample = mInputBuffer[i + framesToSkip] * mGain;
                // Soft clamp to prevent hard clipping distortion
                sample = softClamp(sample);
                outputFloats[i * 2] = sample;
                outputFloats[i * 2 + 1] = sample;
            }
            // Fill remaining with silence
            for (int i = framesToUse; i < numFrames; i++) {
                outputFloats[i * 2] = 0.0f;
                outputFloats[i * 2 + 1] = 0.0f;
            }
        } else if (inputChannelCount == outputChannelCount) {
            int32_t skipSamples = framesToSkip * inputChannelCount;
            for (int i = 0; i < framesToUse * outputChannelCount; i++) {
                float sample = mInputBuffer[i + skipSamples] * mGain;
                outputFloats[i] = softClamp(sample);
            }
            for (int i = framesToUse * outputChannelCount; i < numFrames * outputChannelCount; i++) {
                outputFloats[i] = 0.0f;
            }
        } else {
            // Fallback: fill with silence
            memset(outputFloats, 0, numFrames * outputChannelCount * sizeof(float));
        }

        return oboe::DataCallbackResult::Continue;
    }

private:
    // Soft clamp using tanh-style saturation to prevent hard clipping
    // Keeps signal in -1.0 to 1.0 range with smooth limiting
    inline float softClamp(float x) {
        // Fast approximation: for |x| < 0.9, pass through; above that, soft limit
        if (x > 0.9f) {
            return 0.9f + 0.1f * tanhf((x - 0.9f) * 5.0f);
        } else if (x < -0.9f) {
            return -0.9f + 0.1f * tanhf((x + 0.9f) * 5.0f);
        }
        return x;
    }

    oboe::AudioStream *mInputStream = nullptr;
    oboe::AudioStream *mOutputStream = nullptr;
    float mGain = 3.0f;
    std::vector<float> mInputBuffer;

    // Latency tuning
    int32_t mTargetBufferFrames = 0;  // 0 = disabled (no draining)
    float mDrainRate = 0.0f;          // 0 = disabled, 0.5 = gradual, 1.0 = aggressive
    int32_t mLastAvailableFrames = 0; // For UI display

    // Statistics
    int32_t mCallbackCount = 0;
    int64_t mTotalFramesRead = 0;
    int64_t mTotalFramesWritten = 0;
    int64_t mFramesDrained = 0;
    int32_t mInputXRunCount = 0;
    int32_t mOutputXRunCount = 0;
};

#endif // GUITARPASSTHROUGH_FULLDUPLEXPASS_H
