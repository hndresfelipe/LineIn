#include <jni.h>
#include <android/log.h>
#include "PassthroughEngine.h"

#define LOG_TAG "JNI_Bridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

static PassthroughEngine *sEngine = nullptr;

extern "C" {

JNIEXPORT jboolean JNICALL
Java_dev_andresfelipecaicedo_linein_PassthroughEngine_nativeCreate(JNIEnv *env, jobject thiz) {
    if (sEngine == nullptr) {
        sEngine = new PassthroughEngine();
        LOGI("Native engine created");
        return JNI_TRUE;
    }
    LOGI("Native engine already exists");
    return JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_dev_andresfelipecaicedo_linein_PassthroughEngine_nativeDelete(JNIEnv *env, jobject thiz) {
    if (sEngine != nullptr) {
        delete sEngine;
        sEngine = nullptr;
        LOGI("Native engine deleted");
    }
}

JNIEXPORT void JNICALL
Java_dev_andresfelipecaicedo_linein_PassthroughEngine_nativeSetEffectOn(JNIEnv *env, jobject thiz,
                                                                          jboolean isOn) {
    if (sEngine != nullptr) {
        sEngine->setEffectOn(isOn);
    }
}

JNIEXPORT void JNICALL
Java_dev_andresfelipecaicedo_linein_PassthroughEngine_nativeSetGain(JNIEnv *env, jobject thiz,
                                                                       jfloat gain) {
    if (sEngine != nullptr) {
        sEngine->setGain(gain);
    }
}

JNIEXPORT void JNICALL
Java_dev_andresfelipecaicedo_linein_PassthroughEngine_nativeSetOutputDeviceId(JNIEnv *env, jobject thiz,
                                                                                 jint deviceId) {
    if (sEngine != nullptr) {
        sEngine->setOutputDeviceId(deviceId);
    }
}

JNIEXPORT jboolean JNICALL
Java_dev_andresfelipecaicedo_linein_PassthroughEngine_nativeIsInputMMAP(JNIEnv *env, jobject thiz) {
    if (sEngine != nullptr) {
        return sEngine->isInputMMAP() ? JNI_TRUE : JNI_FALSE;
    }
    return JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_dev_andresfelipecaicedo_linein_PassthroughEngine_nativeIsOutputMMAP(JNIEnv *env, jobject thiz) {
    if (sEngine != nullptr) {
        return sEngine->isOutputMMAP() ? JNI_TRUE : JNI_FALSE;
    }
    return JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_dev_andresfelipecaicedo_linein_PassthroughEngine_nativeGetInputLatencyMs(JNIEnv *env, jobject thiz) {
    if (sEngine != nullptr) {
        return sEngine->getInputLatencyMs();
    }
    return -1;
}

JNIEXPORT jint JNICALL
Java_dev_andresfelipecaicedo_linein_PassthroughEngine_nativeGetOutputLatencyMs(JNIEnv *env, jobject thiz) {
    if (sEngine != nullptr) {
        return sEngine->getOutputLatencyMs();
    }
    return -1;
}

JNIEXPORT void JNICALL
Java_dev_andresfelipecaicedo_linein_PassthroughEngine_nativeSetTargetBufferMs(JNIEnv *env, jobject thiz,
                                                                                  jint ms) {
    if (sEngine != nullptr) {
        sEngine->setTargetBufferMs(ms);
    }
}

JNIEXPORT void JNICALL
Java_dev_andresfelipecaicedo_linein_PassthroughEngine_nativeSetDrainRate(JNIEnv *env, jobject thiz,
                                                                             jfloat rate) {
    if (sEngine != nullptr) {
        sEngine->setDrainRate(rate);
    }
}

JNIEXPORT jint JNICALL
Java_dev_andresfelipecaicedo_linein_PassthroughEngine_nativeGetCurrentBufferMs(JNIEnv *env, jobject thiz) {
    if (sEngine != nullptr) {
        return sEngine->getCurrentBufferMs();
    }
    return -1;
}

} // extern "C"
