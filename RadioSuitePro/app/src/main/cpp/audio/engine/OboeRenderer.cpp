#include "OboeRenderer.h"
#include <oboe/Oboe.h>
#include <android/log.h>
#include <cmath>
#include <algorithm>

#define LOG_TAG "OboeRenderer"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace radiosuite::audio {

OboeRenderer::OboeRenderer(AudioMixer* mixer, AudioPipeline* pipeline)
    : mixer_(mixer), pipeline_(pipeline) {}

OboeRenderer::~OboeRenderer() { stop(); }

bool OboeRenderer::start() {
    if (running_.load()) return true;

    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Output)
           .setPerformanceMode(oboe::PerformanceMode::LowLatency)
           .setSharingMode(oboe::SharingMode::Exclusive)
           .setFormat(oboe::AudioFormat::Float)
           .setChannelCount(oboe::ChannelCount::Stereo)
           .setSampleRate(48000)
           .setDataCallback(this)
           .setErrorCallback(this)
           .setUsage(oboe::Usage::Media)
           .setContentType(oboe::ContentType::Music);

    auto result = builder.openStream(stream_);
    if (result != oboe::Result::OK) {
        LOGE("No se pudo abrir el stream Oboe: %s", oboe::convertToText(result));
        stream_.reset();
        return false;
    }

    const int32_t framesPerBurst = stream_->getFramesPerBurst();
    scratchL_.assign(framesPerBurst * 2, 0.0f);
    scratchR_.assign(framesPerBurst * 2, 0.0f);

    result = stream_->requestStart();
    if (result != oboe::Result::OK) {
        LOGE("No se pudo iniciar el stream: %s", oboe::convertToText(result));
        stream_.reset();
        return false;
    }
    running_.store(true);
    LOGI("Oboe iniciado: sr=%d, burst=%d, perf=%s",
         stream_->getSampleRate(), framesPerBurst,
         oboe::convertToText(stream_->getPerformanceMode()));
    return true;
}

void OboeRenderer::stop() {
    if (!running_.exchange(false)) return;
    if (stream_) {
        stream_->stop();
        stream_->close();
        stream_.reset();
    }
}

oboe::DataCallbackResult OboeRenderer::onAudioReady(
    oboe::AudioStream* stream, void* audioData, int32_t numFrames) {

    const std::size_t n = static_cast<std::size_t>(numFrames);
    if (n > scratchL_.size()) {
        scratchL_.resize(n, 0.0f);
        scratchR_.resize(n, 0.0f);
    }

    // 1) El mezclador produce estéreo en scratch.
    AudioBlock mixed(2, n, scratchL_.data(), scratchR_.data());
    if (mixer_) mixer_->mix(mixed);

    // 2) El pipeline DSP procesa el bloque in-place.
    if (pipeline_) pipeline_->processBlock(mixed);

    // 3) Interleave estéreo float al buffer de Oboe (deinterleaved -> interleaved
    //    si el stream lo exige; Oboe Float Stereo suele ser interleaved).
    float* out = static_cast<float*>(audioData);
    const int32_t ch = stream->getChannelCount();
    if (ch == 2) {
        for (std::size_t i = 0; i < n; ++i) {
            out[i * 2]     = scratchL_[i];
            out[i * 2 + 1] = scratchR_[i];
        }
    } else if (ch == 1) {
        for (std::size_t i = 0; i < n; ++i) {
            out[i] = 0.5f * (scratchL_[i] + scratchR_[i]);
        }
    }

    return oboe::DataCallbackResult::Continue;
}

void OboeRenderer::onErrorBeforeClose(oboe::AudioStream*, oboe::Result error) {
    LOGE("Error Oboe (before close): %s", oboe::convertToText(error));
}

void OboeRenderer::onErrorAfterClose(oboe::AudioStream* stream, oboe::Result error) {
    LOGE("Error Oboe (after close): %s — reiniciando", oboe::convertToText(error));
    // Reconexión: en AAudio los streams pueden desconectarse (p.ej. llamada
    // entrante o cambio de dispositivo). Reabrimos automáticamente.
    if (running_.load()) {
        stream_.reset();
        start();
    }
}

} // namespace radiosuite::audio
