#pragma once

#include <cstddef>

namespace HoopiPi {

/**
 * @brief Simple noise gate with smooth envelope follower
 *
 * Based on NeuralRack's gate implementation. Uses RMS level detection
 * with attack/release smoothing to avoid clicks.
 */
class NoiseGate {
public:
    /**
     * @brief Construct noise gate
     * @param sampleRate Sample rate in Hz
     */
    explicit NoiseGate(int sampleRate);

    /**
     * @brief Process audio buffer in-place
     * @param buffer Audio buffer to process
     * @param numSamples Number of samples
     */
    void process(float* buffer, size_t numSamples);

    /**
     * @brief Set gate threshold
     * @param thresholdDB Threshold in dB (-60 to 0 typical)
     */
    void setThreshold(float thresholdDB);

    /**
     * @brief Get current threshold
     * @return Threshold in dB
     */
    float getThreshold() const { return m_thresholdDB; }

    /**
     * @brief Reset gate state
     */
    void reset();

private:
    int m_sampleRate;
    float m_thresholdDB;
    float m_thresholdLinear;

    // Envelope follower state
    float m_envelope{0.0f};
    float m_gain{1.0f};

    // Time constants (based on NeuralRack)
    static constexpr float ATTACK_MS = 1.0f;
    static constexpr float RELEASE_MS = 100.0f;

    float m_attackCoeff;
    float m_releaseCoeff;

    // Helpers
    float dbToLinear(float db) const;
    void updateCoefficients();
};

} // namespace HoopiPi
