#pragma once

#include <cmath>
#include <atomic>

namespace HoopiPi {

/**
 * @brief Simple 3-band parametric EQ (Bass, Mid, Treble)
 *
 * Uses biquad filters for tone shaping similar to guitar amp tone stacks.
 * Thread-safe with atomic parameter updates.
 */
class ThreeBandEQ {
public:
    ThreeBandEQ(int sampleRate)
        : m_sampleRate(sampleRate)
    {
        reset();
        updateCoefficients();
    }

    /**
     * @brief Process a single sample
     */
    float process(float input) {
        if (!m_enabled.load(std::memory_order_relaxed)) {
            return input;
        }

        // Check if coefficients need updating
        if (m_coeffsDirty.load(std::memory_order_acquire)) {
            updateCoefficients();
            m_coeffsDirty.store(false, std::memory_order_release);
        }

        // Smooth gain changes to avoid clicks/pops (similar to NeuralRack's 0.999 smoothing)
        const float smoothingCoeff = 0.999f;
        m_bassGainSmooth = m_bassGainTarget + smoothingCoeff * (m_bassGainSmooth - m_bassGainTarget);
        m_midGainSmooth = m_midGainTarget + smoothingCoeff * (m_midGainSmooth - m_midGainTarget);
        m_trebleGainSmooth = m_trebleGainTarget + smoothingCoeff * (m_trebleGainSmooth - m_trebleGainTarget);

        float output = input;

        // Bass filter (low shelf)
        output = processBiquad(output, m_bassState, m_bassCoeffs);

        // Mid filter (peaking)
        output = processBiquad(output, m_midState, m_midCoeffs);

        // Treble filter (high shelf)
        output = processBiquad(output, m_trebleState, m_trebleCoeffs);

        return output;
    }

    /**
     * @brief Process a buffer of samples
     */
    void process(const float* input, float* output, size_t numSamples) {
        for (size_t i = 0; i < numSamples; ++i) {
            output[i] = process(input[i]);
        }
    }

    /**
     * @brief Reset filter state
     */
    void reset() {
        for (int i = 0; i < 2; ++i) {
            m_bassState[i] = 0.0f;
            m_midState[i] = 0.0f;
            m_trebleState[i] = 0.0f;
        }
    }

    // ===== Parameter Setters (thread-safe) =====

    void setEnabled(bool enabled) {
        m_enabled.store(enabled, std::memory_order_relaxed);
    }

    void setBass(float db) {
        // Clamp to -20 to +20 dB range (like NeuralRack)
        db = std::max(-20.0f, std::min(20.0f, db));
        m_bass.store(db, std::memory_order_relaxed);
        m_bassGainTarget = std::pow(10.0f, db / 20.0f);
        m_coeffsDirty.store(true, std::memory_order_release);
    }

    void setMid(float db) {
        db = std::max(-20.0f, std::min(20.0f, db));
        m_mid.store(db, std::memory_order_relaxed);
        m_midGainTarget = std::pow(10.0f, db / 20.0f);
        m_coeffsDirty.store(true, std::memory_order_release);
    }

    void setTreble(float db) {
        db = std::max(-20.0f, std::min(20.0f, db));
        m_treble.store(db, std::memory_order_relaxed);
        m_trebleGainTarget = std::pow(10.0f, db / 20.0f);
        m_coeffsDirty.store(true, std::memory_order_release);
    }

    // ===== Parameter Getters =====

    bool getEnabled() const {
        return m_enabled.load(std::memory_order_relaxed);
    }

    float getBass() const {
        return m_bass.load(std::memory_order_relaxed);
    }

    float getMid() const {
        return m_mid.load(std::memory_order_relaxed);
    }

    float getTreble() const {
        return m_treble.load(std::memory_order_relaxed);
    }

private:
    struct BiquadCoeffs {
        float b0, b1, b2, a1, a2;
    };

    float processBiquad(float input, float state[2], const BiquadCoeffs& coeffs) {
        float output = coeffs.b0 * input + state[0];
        state[0] = coeffs.b1 * input - coeffs.a1 * output + state[1];
        state[1] = coeffs.b2 * input - coeffs.a2 * output;
        return output;
    }

    void updateCoefficients() {
        // Bass: Low shelf at 120 Hz
        m_bassCoeffs = calculateLowShelf(120.0f, m_bass.load(), 0.707f);

        // Mid: Peaking at 750 Hz
        m_midCoeffs = calculatePeaking(750.0f, m_mid.load(), 1.0f);

        // Treble: High shelf at 3000 Hz
        m_trebleCoeffs = calculateHighShelf(3000.0f, m_treble.load(), 0.707f);
    }

    BiquadCoeffs calculateLowShelf(float freq, float gainDB, float Q) {
        const float A = std::pow(10.0f, gainDB / 40.0f);
        const float w0 = 2.0f * M_PI * freq / m_sampleRate;
        const float cosw0 = std::cos(w0);
        const float sinw0 = std::sin(w0);
        const float alpha = sinw0 / (2.0f * Q);

        const float a0 = (A + 1.0f) + (A - 1.0f) * cosw0 + 2.0f * std::sqrt(A) * alpha;
        const float a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosw0);
        const float a2 = (A + 1.0f) + (A - 1.0f) * cosw0 - 2.0f * std::sqrt(A) * alpha;
        const float b0 = A * ((A + 1.0f) - (A - 1.0f) * cosw0 + 2.0f * std::sqrt(A) * alpha);
        const float b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosw0);
        const float b2 = A * ((A + 1.0f) - (A - 1.0f) * cosw0 - 2.0f * std::sqrt(A) * alpha);

        return {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
    }

    BiquadCoeffs calculatePeaking(float freq, float gainDB, float Q) {
        const float A = std::pow(10.0f, gainDB / 40.0f);
        const float w0 = 2.0f * M_PI * freq / m_sampleRate;
        const float cosw0 = std::cos(w0);
        const float sinw0 = std::sin(w0);
        const float alpha = sinw0 / (2.0f * Q);

        const float a0 = 1.0f + alpha / A;
        const float a1 = -2.0f * cosw0;
        const float a2 = 1.0f - alpha / A;
        const float b0 = 1.0f + alpha * A;
        const float b1 = -2.0f * cosw0;
        const float b2 = 1.0f - alpha * A;

        return {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
    }

    BiquadCoeffs calculateHighShelf(float freq, float gainDB, float Q) {
        const float A = std::pow(10.0f, gainDB / 40.0f);
        const float w0 = 2.0f * M_PI * freq / m_sampleRate;
        const float cosw0 = std::cos(w0);
        const float sinw0 = std::sin(w0);
        const float alpha = sinw0 / (2.0f * Q);

        const float a0 = (A + 1.0f) - (A - 1.0f) * cosw0 + 2.0f * std::sqrt(A) * alpha;
        const float a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosw0);
        const float a2 = (A + 1.0f) - (A - 1.0f) * cosw0 - 2.0f * std::sqrt(A) * alpha;
        const float b0 = A * ((A + 1.0f) + (A - 1.0f) * cosw0 + 2.0f * std::sqrt(A) * alpha);
        const float b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosw0);
        const float b2 = A * ((A + 1.0f) + (A - 1.0f) * cosw0 - 2.0f * std::sqrt(A) * alpha);

        return {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
    }

    int m_sampleRate;

    // Filter state (x[n-1], x[n-2])
    float m_bassState[2] = {0.0f, 0.0f};
    float m_midState[2] = {0.0f, 0.0f};
    float m_trebleState[2] = {0.0f, 0.0f};

    // Filter coefficients
    BiquadCoeffs m_bassCoeffs{};
    BiquadCoeffs m_midCoeffs{};
    BiquadCoeffs m_trebleCoeffs{};

    // Parameters (atomic for thread safety)
    std::atomic<bool> m_enabled{false};
    std::atomic<float> m_bass{0.0f};
    std::atomic<float> m_mid{0.0f};
    std::atomic<float> m_treble{0.0f};
    std::atomic<bool> m_coeffsDirty{true};

    // Gain smoothing (to avoid clicks/pops when parameters change)
    float m_bassGainTarget{1.0f};
    float m_midGainTarget{1.0f};
    float m_trebleGainTarget{1.0f};
    float m_bassGainSmooth{1.0f};
    float m_midGainSmooth{1.0f};
    float m_trebleGainSmooth{1.0f};
};

} // namespace HoopiPi
