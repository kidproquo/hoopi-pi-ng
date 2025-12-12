#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <cmath>

namespace HoopiPi {

/**
 * @brief Lightweight algorithmic reverb based on Feedback Delay Network (FDN)
 *
 * Uses multi-channel feedback delays with mixing matrices for diffusion.
 * Based on reverb-example-code using signalsmith DSP library.
 * Optimized for guitar amp processing with low latency and CPU usage.
 */
class Reverb {
public:
    /**
     * @brief Construct reverb processor
     * @param sampleRate Sample rate in Hz (typically 48000)
     * @param maxBufferSize Maximum buffer size in samples
     */
    Reverb(int sampleRate, int maxBufferSize);

    /**
     * @brief Destructor
     */
    ~Reverb();

    /**
     * @brief Process stereo audio (in-place supported)
     * @param inputL Left channel input
     * @param inputR Right channel input
     * @param outputL Left channel output
     * @param outputR Right channel output
     * @param numSamples Number of samples to process
     */
    void process(float* inputL, float* inputR, float* outputL, float* outputR, size_t numSamples);

    // ===== Enable/Disable =====

    /**
     * @brief Enable/disable reverb processing
     * @param enabled true to enable, false to bypass
     */
    void setEnabled(bool enabled);

    /**
     * @brief Get enabled state
     * @return true if enabled
     */
    bool getEnabled() const;

    // ===== Parameters =====

    /**
     * @brief Set room size
     * @param size Room size (0.0 = small room ~20ms, 1.0 = large hall ~200ms)
     */
    void setRoomSize(float size);

    /**
     * @brief Set decay time (RT60 reverb time)
     * @param seconds Decay time in seconds (0.1-10.0)
     */
    void setDecayTime(float seconds);

    /**
     * @brief Set dry/wet mix
     * @param dry Dry signal level (0.0-1.0)
     * @param wet Wet signal level (0.0-1.0)
     */
    void setMix(float dry, float wet);

    /**
     * @brief Get room size
     * @return Room size (0.0-1.0)
     */
    float getRoomSize() const;

    /**
     * @brief Get decay time
     * @return Decay time in seconds
     */
    float getDecayTime() const;

    /**
     * @brief Get dry level
     * @return Dry level (0.0-1.0)
     */
    float getDry() const;

    /**
     * @brief Get wet level
     * @return Wet level (0.0-1.0)
     */
    float getWet() const;

    /**
     * @brief Clear all internal buffers (removes reverb tail)
     */
    void clearBuffers();

    /**
     * @brief Set sample rate (triggers reconfiguration)
     * @param sampleRate New sample rate in Hz
     */
    void setSampleRate(int sampleRate);

    /**
     * @brief Get current sample rate
     * @return Sample rate in Hz
     */
    int getSampleRate() const;

private:
    // Forward declare implementation to hide DSP details
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    std::atomic<bool> m_enabled{false};
    int m_sampleRate;
    int m_maxBufferSize;

    // Parameter storage
    std::atomic<float> m_roomSize{0.3f};      // 30% = ~60ms (small room)
    std::atomic<float> m_decayTime{2.0f};     // 2 seconds RT60
    std::atomic<float> m_dry{1.0f};           // 100% dry by default
    std::atomic<float> m_wet{0.3f};           // 30% wet by default

    // Reconfigure internal delays based on current parameters
    void reconfigure();
};

} // namespace HoopiPi
