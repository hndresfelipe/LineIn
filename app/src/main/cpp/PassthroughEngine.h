#ifndef GUITARPASSTHROUGH_PASSTHROUGHENGINE_H
#define GUITARPASSTHROUGH_PASSTHROUGHENGINE_H

#include <oboe/Oboe.h>
#include "FullDuplexPass.h"

class PassthroughEngine : public oboe::AudioStreamErrorCallback {
public:
    PassthroughEngine();
    ~PassthroughEngine();

    void setEffectOn(bool isOn);
    void setGain(float gain);
    void setOutputDeviceId(int32_t deviceId);

    bool isInputMMAP() const;
    bool isOutputMMAP() const;
    int32_t getInputLatencyMs() const;
    int32_t getOutputLatencyMs() const;

    // Latency tuning
    void setTargetBufferMs(int32_t ms);
    void setDrainRate(float rate);
    int32_t getCurrentBufferMs() const;

    // ErrorCallback methods
    void onErrorBeforeClose(oboe::AudioStream *stream, oboe::Result result) override;
    void onErrorAfterClose(oboe::AudioStream *stream, oboe::Result result) override;

private:
    bool openStreams();
    void closeStreams();
    void restartStreams();

    std::shared_ptr<oboe::AudioStream> mInputStream;
    std::shared_ptr<oboe::AudioStream> mOutputStream;
    std::unique_ptr<FullDuplexPass> mFullDuplexPass;

    int32_t mSampleRate = oboe::kUnspecified;
    int32_t mInputChannelCount = oboe::ChannelCount::Mono;  // iRig HD 2 is mono
    int32_t mOutputChannelCount = oboe::ChannelCount::Stereo;
    int32_t mOutputDeviceId = oboe::kUnspecified;
    bool mInputUsesMMAP = false;
    bool mOutputUsesMMAP = false;
    bool mIsEffectOn = false;
    std::mutex mRestartMutex;
};

#endif // GUITARPASSTHROUGH_PASSTHROUGHENGINE_H
