#include "NativeEngine.h"
#include <jni.h>
#include <android/log.h>
#include <memory>
#include <unordered_map>
#include <mutex>

#define LOG_TAG "RadioSuiteJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using namespace radiosuite;

namespace {
// Registro global de instancias de NativeEngine por jlong handle.
// En una app real esto vive en el contexto del Foreground Service.
std::mutex g_engineMutex;
std::unordered_map<jlong, std::unique_ptr<jni::NativeEngine>> g_engines;
jlong g_nextHandle = 1;

jlong createEngine(JNIEnv*) {
    std::lock_guard<std::mutex> lk(g_engineMutex);
    jlong h = g_nextHandle++;
    g_engines[h] = std::make_unique<jni::NativeEngine>();
    return h;
}

jni::NativeEngine* engine(JNIEnv* env, jlong handle) {
    std::lock_guard<std::mutex> lk(g_engineMutex);
    auto it = g_engines.find(handle);
    return (it != g_engines.end()) ? it->second.get() : nullptr;
}

void destroyEngine(JNIEnv* env, jlong handle) {
    std::lock_guard<std::mutex> lk(g_engineMutex);
    g_engines.erase(handle);
}
} // namespace

extern "C" {

// ---------------- Engine lifecycle ----------------
JNIEXPORT jlong JNICALL
Java_com_radiosuite_pro_dsp_DspEngine_nativeCreate(JNIEnv* env, jclass) {
    return createEngine(env);
}

JNIEXPORT void JNICALL
Java_com_radiosuite_pro_dsp_DspEngine_nativeDestroy(JNIEnv* env, jclass, jlong handle) {
    destroyEngine(env, handle);
}

JNIEXPORT jboolean JNICALL
Java_com_radiosuite_pro_dsp_DspEngine_nativeStart(JNIEnv* env, jclass, jlong handle) {
    auto* e = engine(env, handle);
    return e ? static_cast<jboolean>(e->start()) : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_radiosuite_pro_dsp_DspEngine_nativeStop(JNIEnv* env, jclass, jlong handle) {
    auto* e = engine(env, handle);
    if (e) e->stop();
}

// ---------------- DSP parameters ----------------
JNIEXPORT void JNICALL
Java_com_radiosuite_pro_dsp_DspEngine_nativeUpdatePipeline(
    JNIEnv* env, jclass, jlong handle,
    jfloat targetDb, jfloat maxGainDb, jfloat agcAttack, jfloat agcRelease,
    jfloat outGainDb, jboolean enabled) {
    auto* e = engine(env, handle);
    if (!e) return;
    audio::PipelineParameters p{};
    p.agc.targetLevelDb = targetDb;
    p.agc.maxGainDb     = maxGainDb;
    p.agc.attackMs      = agcAttack;
    p.agc.releaseMs     = agcRelease;
    p.outputGainDb      = outGainDb;
    e->updatePipelineParameters(p);
    e->setPipelineEnabled(enabled == JNI_TRUE);
}

// ---------------- Metrics (polling desde UI) ----------------
JNIEXPORT jfloat JNICALL
Java_com_radiosuite_pro_dsp_DspEngine_nativeInputPeakDb(JNIEnv* env, jclass, jlong h) {
    auto* e = engine(env, h);
    return e ? e->metrics().inputPeakDb.load() : -120.0f;
}
JNIEXPORT jfloat JNICALL
Java_com_radiosuite_pro_dsp_DspEngine_nativeOutputPeakDb(JNIEnv* env, jclass, jlong h) {
    auto* e = engine(env, h);
    return e ? e->metrics().outputPeakDb.load() : -120.0f;
}
JNIEXPORT jfloat JNICALL
Java_com_radiosuite_pro_dsp_DspEngine_nativeOutputRmsDb(JNIEnv* env, jclass, jlong h) {
    auto* e = engine(env, h);
    return e ? e->metrics().outputRmsDb.load() : -120.0f;
}
JNIEXPORT jfloat JNICALL
Java_com_radiosuite_pro_dsp_DspEngine_nativeAgcGainDb(JNIEnv* env, jclass, jlong h) {
    auto* e = engine(env, h);
    return e ? e->metrics().agcGainDb.load() : 0.0f;
}
JNIEXPORT jfloat JNICALL
Java_com_radiosuite_pro_dsp_DspEngine_nativeBrickwallGrDb(JNIEnv* env, jclass, jlong h) {
    auto* e = engine(env, h);
    return e ? e->metrics().brickwallGrDb.load() : 0.0f;
}

// ---------------- Streaming ----------------
JNIEXPORT jboolean JNICALL
Java_com_radiosuite_pro_stream_StreamEncoder_nativeStartEncoder(
    JNIEnv* env, jclass, jlong handle, jint codec, jint sampleRate,
    jint channels, jint bitrate) {
    auto* e = engine(env, handle);
    if (!e) return JNI_FALSE;
    audio::EncoderParams p;
    p.codec      = static_cast<audio::StreamCodec>(codec);
    p.sampleRate = sampleRate;
    p.channels   = channels;
    p.bitrate    = bitrate;
    e->setEncoderParams(p);
    // El callback de paquetes se conecta desde Kotlin vía IcecastClient.
    return static_cast<jboolean>(e->startEncoder(nullptr));
}

JNIEXPORT void JNICALL
Java_com_radiosuite_pro_stream_StreamEncoder_nativeStopEncoder(
    JNIEnv* env, jclass, jlong handle) {
    auto* e = engine(env, handle);
    if (e) e->stopEncoder();
}

} // extern "C"
