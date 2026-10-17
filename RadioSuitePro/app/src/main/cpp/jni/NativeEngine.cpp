#include "NativeEngine.h"
#include <android/log.h>

#define LOG_TAG "NativeEngine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace radiosuite::jni {

NativeEngine::NativeEngine() {
    const float sr = 48000.0f;
    pipeline_  = std::make_unique<audio::AudioPipeline>(sr, 5, 4);
    mixer_     = std::make_unique<audio::AudioMixer>(sr);
    renderer_  = std::make_unique<audio::OboeRenderer>(mixer_.get(), pipeline_.get());
    LOGI("NativeEngine construido (sr=%.0f)", sr);
}

NativeEngine::~NativeEngine() {
    stop();
    LOGI("NativeEngine destruido");
}

bool NativeEngine::start() {
    if (!renderer_->start()) { LOGE("OboeRenderer no arrancó"); return false; }
    LOGI("Motor nativo iniciado");
    return true;
}

void NativeEngine::stop() {
    stopEncoder();
    renderer_->stop();
}

void NativeEngine::updatePipelineParameters(const audio::PipelineParameters& p) {
    pipeline_->updateParameters(p);
}

void NativeEngine::setPipelineEnabled(bool enabled) {
    pipeline_->setEnabled(enabled);
}

const audio::PipelineMetrics& NativeEngine::metrics() const {
    return pipeline_->metrics();
}

void NativeEngine::setSourceGain(std::size_t id, float gainDb) {
    // La fuente se registra desde Kotlin via setSource(); aquí solo ajustamos gain.
    // (El puente JNI de fuentes se añade cuando se integra ExoPlayer.)
    (void) id; (void) gainDb;
}

void NativeEngine::startCrossfade(float durationMs) {
    mixer_->startCrossfade(durationMs);
}

void NativeEngine::setEncoderParams(const audio::EncoderParams& p) {
    if (encoder_ && encoder_->isRunning()) {
        encoder_->stop();
        encoder_.reset();
    }
    encoder_ = std::make_unique<audio::IcecastEncoder>(p);
}

bool NativeEngine::startEncoder(audio::PacketCallback cb) {
    if (!encoder_) {
        LOGE("setEncoderParams() no llamado antes de startEncoder()");
        return false;
    }
    return encoder_->start(cb);
}

void NativeEngine::stopEncoder() {
    if (encoder_) { encoder_->stop(); encoder_.reset(); }
}

void NativeEngine::pushPlayerPcm(const float* left, const float* right, std::size_t n) {
    // Alimenta el codificador directamente desde el post-pipeline.
    // En la integración completa, el reproductor alimenta el mezclador, no aquí.
    if (encoder_) encoder_->pushSamples(left, right, n);
}

} // namespace radiosuite::jni
