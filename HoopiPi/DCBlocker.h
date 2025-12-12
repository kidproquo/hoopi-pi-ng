#pragma once

#include <cstddef>

namespace HoopiPi {

/**
 * @brief DC blocking filter (high-pass filter at ~10Hz)
 *
 * Removes DC offset that can accumulate from neural model processing.
 * Uses a simple first-order IIR filter.
 */
class DCBlocker {
public:
    /**
     * @brief Construct DC blocker
     * @param sampleRate Sample rate in Hz
     */
    explicit DCBlocker(int sampleRate);

    /**
     * @brief Process audio buffer in-place
     * @param buffer Audio buffer to process
     * @param numSamples Number of samples
     */
    void process(float* buffer, size_t numSamples);

    /**
     * @brief Reset filter state
     */
    void reset();

private:
    // Filter state
    float m_x1{0.0f}; // Previous input
    float m_y1{0.0f}; // Previous output

    // Filter coefficient
    float m_coefficient;

    // DC blocking frequency (Hz)
    static constexpr float DC_BLOCK_FREQ = 10.0f;
};

} // namespace HoopiPi
