#include "NoiseGate.h"
#include <cmath>
#include <algorithm>

namespace HoopiPi {

NoiseGate::NoiseGate(int sampleRate)
    : m_sampleRate(sampleRate)
    , m_thresholdDB(-40.0f)
{
    updateCoefficients();
    m_thresholdLinear = dbToLinear(m_thresholdDB);
}

void NoiseGate::process(float* buffer, size_t numSamples) {
    for (size_t i = 0; i < numSamples; ++i) {
        float input = buffer[i];
        float inputAbs = std::abs(input);

        // Update envelope follower (simple peak detector with smoothing)
        if (inputAbs > m_envelope) {
            // Attack
            m_envelope = m_envelope * m_attackCoeff + inputAbs * (1.0f - m_attackCoeff);
        } else {
            // Release
            m_envelope = m_envelope * m_releaseCoeff + inputAbs * (1.0f - m_releaseCoeff);
        }

        // Calculate gate gain (simple hard gate)
        if (m_envelope > m_thresholdLinear) {
            m_gain = 1.0f;
        } else {
            m_gain = 0.0f;
        }

        // Apply gain
        buffer[i] = input * m_gain;
    }
}

void NoiseGate::setThreshold(float thresholdDB) {
    m_thresholdDB = thresholdDB;
    m_thresholdLinear = dbToLinear(thresholdDB);
}

void NoiseGate::reset() {
    m_envelope = 0.0f;
    m_gain = 1.0f;
}

float NoiseGate::dbToLinear(float db) const {
    return std::pow(10.0f, db / 20.0f);
}

void NoiseGate::updateCoefficients() {
    // Convert time constants to filter coefficients
    // coeff = exp(-1 / (time_ms * sample_rate / 1000))
    m_attackCoeff = std::exp(-1.0f / (ATTACK_MS * m_sampleRate / 1000.0f));
    m_releaseCoeff = std::exp(-1.0f / (RELEASE_MS * m_sampleRate / 1000.0f));
}

} // namespace HoopiPi
