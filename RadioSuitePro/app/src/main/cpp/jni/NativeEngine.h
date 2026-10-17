#pragma once

#include <jni.h>
#include "../audio/AudioPipeline.h"
#include "../stream/IcecastEncoder.h"
#include "../player/AudioMixer.h"
#include "../audio/engine/OboeRenderer.h"
#include <memory>

namespace radiosuite::jni {

// Posee la totalidad del motor nativo (mezclador + pipeline + render + encoder)
// y expone handlers estables a Kotlin. Una sola instancia vive mientras el
// Foreground Service esté activo.
class NativeEngine {
public:
    NativeEngine();
    ~NativeEngine();

    // ---- Ciclo de vida ----
    bool start();
    void stop();

    // ---- Pipeline DSP ----
    void updatePipelineParameters(const audio::PipelineParameters& p);
    void setPipelineEnabled(bool enabled);
    const audio::PipelineMetrics& metrics() const;

    // ---- Mezclador / fuentes ----
    void setSourceGain(std::size_t id, float gainDb);
    void startCrossfade(float durationMs);

    // ---- Streaming ----
    void setEncoderParams(const audio::EncoderParams& p);
    bool startEncoder(audio::PacketCallback cb);
    void stopEncoder();

    // Inyecta PCM de un reproductor Kotlin (ExoPlayer) hacia el mezclador.
    void pushPlayerPcm(const float* left, const float* right, std::size_t n);

private:
    std::unique_ptr<audio::AudioMixer>     mixer_;
    std::unique_ptr<audio::AudioPipeline>  pipeline_;
    std::unique_ptr<audio::OboeRenderer>   renderer_;
    std::unique_ptr<audio::IcecastEncoder> encoder_;
};

} // namespace radiosuite::jni
