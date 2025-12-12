#include "DCBlocker.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace HoopiPi {

DCBlocker::DCBlocker(int sampleRate) {
    // Calculate coefficient for DC blocking filter
    // y[n] = x[n] - x[n-1] + R * y[n-1]
    // where R = 1 - (2 * pi * fc / fs)
    float fc = DC_BLOCK_FREQ;
    float fs = static_cast<float>(sampleRate);
    m_coefficient = 1.0f - (2.0f * M_PI * fc / fs);

    reset();
}

void DCBlocker::process(float* buffer, size_t numSamples) {
    for (size_t i = 0; i < numSamples; ++i) {
        float input = buffer[i];

        // DC blocking filter: y[n] = x[n] - x[n-1] + R * y[n-1]
        float output = input - m_x1 + m_coefficient * m_y1;

        // Update state
        m_x1 = input;
        m_y1 = output;

        buffer[i] = output;
    }
}

void DCBlocker::reset() {
    m_x1 = 0.0f;
    m_y1 = 0.0f;
}

} // namespace HoopiPi
