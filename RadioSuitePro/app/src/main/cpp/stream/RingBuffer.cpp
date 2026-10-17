#include "RingBuffer.h"
#include <algorithm>

namespace radiosuite::audio {

RingBuffer::RingBuffer(std::size_t capacitySamples)
    : mask_(0) {
    // Redondea a la siguiente potencia de 2 para usar & en vez de %.
    std::size_t cap = 1;
    while (cap < capacitySamples) cap <<= 1;
    buffer_.resize(cap, 0.0f);
    mask_ = cap - 1;
}

std::size_t RingBuffer::available() const {
    const std::size_t w = writePos_.load(std::memory_order_acquire);
    const std::size_t r = readPos_.load(std::memory_order_acquire);
    return (w - r) & mask_; // no distingue lleno vs vacío con diferencia exacta;
                            // usamos w-r como conteo (nunca igual a cap).
}

std::size_t RingBuffer::write(const float* src, std::size_t n) {
    const std::size_t cap = mask_ + 1;
    std::size_t w = writePos_.load(std::memory_order_relaxed);
    std::size_t r = readPos_.load(std::memory_order_acquire);
    const std::size_t freeSamples = cap - 1 - ((w - r) & mask_);
    const std::size_t toWrite = std::min(n, freeSamples);
    for (std::size_t i = 0; i < toWrite; ++i) {
        buffer_[(w + i) & mask_] = src[i];
    }
    writePos_.store((w + toWrite) & mask_, std::memory_order_release);
    return toWrite;
}

std::size_t RingBuffer::read(float* dst, std::size_t n) {
    std::size_t r = readPos_.load(std::memory_order_relaxed);
    std::size_t w = writePos_.load(std::memory_order_acquire);
    const std::size_t avail = (w - r) & mask_;
    const std::size_t toRead = std::min(n, avail);
    for (std::size_t i = 0; i < toRead; ++i) {
        dst[i] = buffer_[(r + i) & mask_];
    }
    readPos_.store((r + toRead) & mask_, std::memory_order_release);
    return toRead;
}

void RingBuffer::clear() {
    writePos_.store(0, std::memory_order_release);
    readPos_.store(0, std::memory_order_release);
    std::fill(buffer_.begin(), buffer_.end(), 0.0f);
}

} // namespace radiosuite::audio
