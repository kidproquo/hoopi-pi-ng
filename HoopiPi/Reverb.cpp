#include "Reverb.h"
#include "../dependencies/dsp/delay.h"
#include "../dependencies/dsp/mix.h"
#include <cstring>
#include <algorithm>
#include <random>

namespace HoopiPi {

// Number of delay lines in the feedback network
constexpr int kNumChannels = 8;
constexpr int kDiffusionSteps = 4;

// Delay type: using linear interpolation for smoother sound
using Delay = signalsmith::delay::Delay<float>;

/**
 * @brief Diffusion step for early reflections
 *
 * Implements a single stage of allpass diffusion using delays and a Hadamard matrix.
 * Randomizes delay times and polarities for natural sound.
 */
struct DiffusionStep {
    using Array = std::array<float, kNumChannels>;

    float delayMsRange = 10.0f;
    std::array<int, kNumChannels> delaySamples;
    std::array<Delay, kNumChannels> delays;
    std::array<bool, kNumChannels> flipPolarity;

    void configure(double sampleRate, uint32_t seed) {
        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        float delaySamplesRange = delayMsRange * 0.001f * sampleRate;
        for (int c = 0; c < kNumChannels; ++c) {
            float rangeLow = delaySamplesRange * c / kNumChannels;
            float rangeHigh = delaySamplesRange * (c + 1) / kNumChannels;
            delaySamples[c] = rangeLow + dist(rng) * (rangeHigh - rangeLow);
            delays[c].resize(delaySamples[c] + 1);
            delays[c].reset();
            flipPolarity[c] = (dist(rng) > 0.5f);
        }
    }

    void reset() {
        for (auto& delay : delays) {
            delay.reset();
        }
    }

    Array process(const Array& input) {
        // Write input to delays
        Array delayed;
        for (int c = 0; c < kNumChannels; ++c) {
            delays[c].write(input[c]);
            delayed[c] = delays[c].read(delaySamples[c]);
        }

        // Mix with Hadamard matrix for diffusion
        Array mixed = delayed;
        signalsmith::mix::Hadamard<float, kNumChannels>::inPlace(mixed.data());

        // Flip some polarities for better decorrelation
        for (int c = 0; c < kNumChannels; ++c) {
            if (flipPolarity[c]) mixed[c] *= -1.0f;
        }

        return mixed;
    }
};

/**
 * @brief Multi-stage diffuser with decreasing delay lengths
 *
 * Creates dense early reflections by cascading diffusion steps
 * with progressively shorter delay times.
 */
struct Diffuser {
    using Array = std::array<float, kNumChannels>;

    std::array<DiffusionStep, kDiffusionSteps> steps;

    void configure(double sampleRate, float totalDiffusionMs) {
        float diffusionMs = totalDiffusionMs;
        for (int i = 0; i < kDiffusionSteps; ++i) {
            diffusionMs *= 0.5f; // Each step is half the previous
            steps[i].delayMsRange = diffusionMs;
            steps[i].configure(sampleRate, 12345 + i * 6789);
        }
    }

    void reset() {
        for (auto& step : steps) {
            step.reset();
        }
    }

    Array process(const Array& samples) {
        Array output = samples;
        for (auto& step : steps) {
            output = step.process(output);
        }
        return output;
    }
};

/**
 * @brief Feedback delay network for late reverberation
 *
 * Uses multiple delay lines with a Householder mixing matrix
 * to create dense, diffuse reverberation tail.
 */
struct FeedbackNetwork {
    using Array = std::array<float, kNumChannels>;

    float delayMs = 100.0f;
    float decayGain = 0.85f;

    std::array<int, kNumChannels> delaySamples;
    std::array<Delay, kNumChannels> delays;

    void configure(double sampleRate) {
        float delaySamplesBase = delayMs * 0.001f * sampleRate;
        for (int c = 0; c < kNumChannels; ++c) {
            // Distribute delay times exponentially between delayMs and 2*delayMs
            float r = c * 1.0f / kNumChannels;
            delaySamples[c] = std::pow(2.0f, r) * delaySamplesBase;
            delays[c].resize(delaySamples[c] + 1);
            delays[c].reset();
        }
    }

    void reset() {
        for (auto& delay : delays) {
            delay.reset();
        }
    }

    Array process(const Array& input) {
        // Read delayed samples
        Array delayed;
        for (int c = 0; c < kNumChannels; ++c) {
            delayed[c] = delays[c].read(delaySamples[c]);
        }

        // Mix using Householder matrix
        Array mixed = delayed;
        signalsmith::mix::Householder<float, kNumChannels>::inPlace(mixed.data());

        // Write feedback sum to delays
        for (int c = 0; c < kNumChannels; ++c) {
            float sum = input[c] + mixed[c] * decayGain;
            delays[c].write(sum);
        }

        return delayed;
    }
};

/**
 * @brief Complete reverb implementation
 *
 * Combines diffuser for early reflections with feedback network
 * for late reverberation.
 */
struct Reverb::Impl {
    using Array = std::array<float, kNumChannels>;

    Diffuser diffuser;
    FeedbackNetwork feedback;

    float dry = 1.0f;
    float wet = 0.3f;

    void configure(double sampleRate, float roomSizeMs, float rt60, float dryLevel, float wetLevel) {
        dry = dryLevel;
        wet = wetLevel;

        feedback.delayMs = roomSizeMs;

        // Calculate decay gain for desired RT60
        // How long does the signal take to loop through feedback?
        float typicalLoopMs = roomSizeMs * 1.5f;
        // How many loops during RT60 period?
        float loopsPerRt60 = rt60 / (typicalLoopMs * 0.001f);
        // dB reduction per loop
        float dbPerCycle = -60.0f / loopsPerRt60;
        feedback.decayGain = std::pow(10.0f, dbPerCycle * 0.05f);

        // Configure diffuser for early reflections
        diffuser.configure(sampleRate, roomSizeMs);
        feedback.configure(sampleRate);
    }

    void reset() {
        diffuser.reset();
        feedback.reset();
    }

    void processMono(float input, float& outputL, float& outputR) {
        // Spread mono input across channels
        Array multichannel;
        for (int c = 0; c < kNumChannels; ++c) {
            multichannel[c] = input;
        }

        // Process through diffuser and feedback
        Array diffuse = diffuser.process(multichannel);
        Array longLasting = feedback.process(diffuse);

        // Mix down to stereo
        float leftSum = 0.0f, rightSum = 0.0f;
        for (int c = 0; c < kNumChannels; ++c) {
            if (c % 2 == 0) {
                leftSum += longLasting[c];
            } else {
                rightSum += longLasting[c];
            }
        }

        // Apply dry/wet mix
        outputL = dry * input + wet * leftSum / (kNumChannels / 2);
        outputR = dry * input + wet * rightSum / (kNumChannels / 2);
    }

    void processStereo(float inputL, float inputR, float& outputL, float& outputR) {
        // Distribute stereo input across channels
        Array multichannel;
        for (int c = 0; c < kNumChannels; ++c) {
            multichannel[c] = (c % 2 == 0) ? inputL : inputR;
        }

        // Process through diffuser and feedback
        Array diffuse = diffuser.process(multichannel);
        Array longLasting = feedback.process(diffuse);

        // Mix down to stereo
        float leftSum = 0.0f, rightSum = 0.0f;
        for (int c = 0; c < kNumChannels; ++c) {
            if (c % 2 == 0) {
                leftSum += longLasting[c];
            } else {
                rightSum += longLasting[c];
            }
        }

        // Apply dry/wet mix
        outputL = dry * inputL + wet * leftSum / (kNumChannels / 2);
        outputR = dry * inputR + wet * rightSum / (kNumChannels / 2);
    }
};

// ===== Reverb Class Implementation =====

Reverb::Reverb(int sampleRate, int maxBufferSize)
    : m_sampleRate(sampleRate)
    , m_maxBufferSize(maxBufferSize)
{
    m_impl = std::make_unique<Impl>();
    reconfigure();
}

Reverb::~Reverb() = default;

void Reverb::process(float* inputL, float* inputR, float* outputL, float* outputR, size_t numSamples) {
    if (!m_enabled.load(std::memory_order_relaxed)) {
        // Bypass: copy input to output
        if (inputL != outputL) {
            std::memcpy(outputL, inputL, numSamples * sizeof(float));
        }
        if (inputR != outputR) {
            std::memcpy(outputR, inputR, numSamples * sizeof(float));
        }
        return;
    }

    // Process each sample
    for (size_t i = 0; i < numSamples; ++i) {
        m_impl->processStereo(inputL[i], inputR[i], outputL[i], outputR[i]);
    }
}

void Reverb::setEnabled(bool enabled) {
    m_enabled.store(enabled, std::memory_order_relaxed);
}

bool Reverb::getEnabled() const {
    return m_enabled.load(std::memory_order_relaxed);
}

void Reverb::setRoomSize(float size) {
    size = std::max(0.0f, std::min(1.0f, size));
    m_roomSize.store(size, std::memory_order_relaxed);
    reconfigure();
}

void Reverb::setDecayTime(float seconds) {
    seconds = std::max(0.1f, std::min(10.0f, seconds));
    m_decayTime.store(seconds, std::memory_order_relaxed);
    reconfigure();
}

void Reverb::setMix(float dry, float wet) {
    m_dry.store(std::max(0.0f, std::min(1.0f, dry)), std::memory_order_relaxed);
    m_wet.store(std::max(0.0f, std::min(1.0f, wet)), std::memory_order_relaxed);
    reconfigure();
}

float Reverb::getRoomSize() const {
    return m_roomSize.load(std::memory_order_relaxed);
}

float Reverb::getDecayTime() const {
    return m_decayTime.load(std::memory_order_relaxed);
}

float Reverb::getDry() const {
    return m_dry.load(std::memory_order_relaxed);
}

float Reverb::getWet() const {
    return m_wet.load(std::memory_order_relaxed);
}

void Reverb::clearBuffers() {
    m_impl->reset();
}

void Reverb::setSampleRate(int sampleRate) {
    m_sampleRate = sampleRate;
    reconfigure();
}

int Reverb::getSampleRate() const {
    return m_sampleRate;
}

void Reverb::reconfigure() {
    // Map room size (0-1) to delay time (20ms-200ms)
    float size = m_roomSize.load(std::memory_order_relaxed);
    float roomSizeMs = 20.0f + size * 180.0f;

    float rt60 = m_decayTime.load(std::memory_order_relaxed);
    float dry = m_dry.load(std::memory_order_relaxed);
    float wet = m_wet.load(std::memory_order_relaxed);

    m_impl->configure(m_sampleRate, roomSizeMs, rt60, dry, wet);
}

} // namespace HoopiPi
