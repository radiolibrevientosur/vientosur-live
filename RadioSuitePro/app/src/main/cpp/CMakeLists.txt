# CMakeLists.txt — RadioSuitePro motor nativo (NDK)
cmake_minimum_required(VERSION 3.22.1)
project(radiosuite C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Oboe (AAudio / OpenSL ES) ---
set(OBOE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../libs/oboe)
if(EXISTS ${OBOE_DIR})
    add_subdirectory(${OBOE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/oboe-build)
else()
    # En build real se obtiene via Prefab / dependency de Gradle.
    message(WARNING "Oboe no encontrado en ${OBOE_DIR}; se asume Prefab de Gradle.")
endif()

# --- Bibliotecas de codec (libogg, libvorbis, libopus, libmp3lame) ---
# En producción se precompilan como prebuilt static libs para cada ABI.
set(CODEC_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../libs/codecs)
add_library(ogg     STATIC IMPORTED)
add_library(vorbis  STATIC IMPORTED)
add_library(vorbisenc STATIC IMPORTED)
add_library(opus    STATIC IMPORTED)
add_library(mp3lame STATIC IMPORTED)
set_target_properties(ogg     PROPERTIES IMPORTED_LOCATION ${CODEC_LIB_DIR}/libogg.a)
set_target_properties(vorbis  PROPERTIES IMPORTED_LOCATION ${CODEC_LIB_DIR}/libvorbis.a)
set_target_properties(vorbisenc PROPERTIES IMPORTED_LOCATION ${CODEC_LIB_DIR}/libvorbisenc.a)
set_target_properties(opus    PROPERTIES IMPORTED_LOCATION ${CODEC_LIB_DIR}/libopus.a)
set_target_properties(mp3lame PROPERTIES IMPORTED_LOCATION ${CODEC_LIB_DIR}/libmp3lame.a)

# --- Biblioteca principal ---
add_library(radiosuite-native SHARED
    jni/jni_bridge.cpp
    audio/AudioPipeline.cpp
    audio/processors/Agc.cpp
    audio/processors/ParametricEq.cpp
    audio/processors/MultibandComp.cpp
    audio/processors/StereoEnhancer.cpp
    audio/processors/BrickwallLimiter.cpp
    audio/dsp/Biquad.cpp
    player/AudioMixer.cpp
    audio/engine/OboeRenderer.cpp
    stream/IcecastEncoder.cpp
    stream/RingBuffer.cpp
    jni/NativeEngine.cpp
)

target_include_directories(radiosuite-native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OBOE_DIR}/include
    ${CODEC_LIB_DIR}/include
)

find_library(log-lib log)
find_library(android-lib android)

target_link_libraries(radiosuite-native
    oboe
    ogg vorbis vorbisenc opus mp3lame
    ${log-lib}
    ${android-lib}
)

# Optimizaciones de tiempo real: sin excepciones, sin RTTI, -O3.
target_compile_options(radiosuite-native PRIVATE
    -O3 -ffast-math -fno-exceptions -fno-rtti
    -Wall -Wextra -Wno-unused-parameter
)
