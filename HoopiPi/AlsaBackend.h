#pragma once

#include <alsa/asoundlib.h>
#include <string>
#include <functional>
#include <atomic>
#include <thread>

namespace HoopiPi {

class Engine;

/**
 * @brief ALSA audio backend for real-time processing
 *
 * Handles ALSA PCM setup and real-time audio I/O thread.
 * Supports hw:DaisySeed and other ALSA devices.
 */
class AlsaBackend {
public:
    /**
     * @brief Construct ALSA backend
     * @param engine Pointer to processing engine
     */
    explicit AlsaBackend(Engine* engine);

    /**
     * @brief Destructor - ensures clean shutdown
     */
    ~AlsaBackend();

    /**
     * @brief Initialize ALSA and open device
     * @param deviceName ALSA device name (e.g., "hw:DaisySeed")
     * @param sampleRate Sample rate in Hz
     * @param periodSize Period size in frames
     * @param numPeriods Number of periods in buffer
     * @return true if successful
     */
    bool init(const std::string& deviceName, int sampleRate,
              int periodSize, int numPeriods = 2);

    /**
     * @brief Start audio processing thread
     * @return true if successful
     */
    bool start();

    /**
     * @brief Stop audio processing thread
     */
    void stop();

    /**
     * @brief Check if backend is running
     * @return true if audio thread is running
     */
    bool isRunning() const { return m_running.load(std::memory_order_acquire); }

    /**
     * @brief Get current xrun count
     * @return Number of xruns since start
     */
    uint32_t getXrunCount() const { return m_xrunCount.load(std::memory_order_relaxed); }

    /**
     * @brief Reset xrun counter
     */
    void resetXrunCount() { m_xrunCount.store(0, std::memory_order_relaxed); }

    /**
     * @brief Get actual sample rate
     * @return Sample rate in Hz
     */
    int getSampleRate() const { return m_sampleRate; }

    /**
     * @brief Get period size
     * @return Period size in frames
     */
    int getPeriodSize() const { return m_periodSize; }

    /**
     * @brief Get latency in milliseconds
     * @return Latency in ms
     */
    float getLatencyMs() const;

private:
    // ALSA setup
    bool setupPCM(const std::string& deviceName);
    bool setHWParams();
    bool setSWParams();
    void closePCM();

    // Audio thread
    void audioThread();
    int audioCallback();

    // Format conversion
    void int16ToFloat(const int16_t* input, float* output, size_t numSamples);
    void floatToInt16(const float* input, int16_t* output, size_t numSamples);

    // Error recovery
    int xrunRecovery(int err);

    // Engine
    Engine* m_engine;

    // ALSA handles
    snd_pcm_t* m_captureHandle{nullptr};
    snd_pcm_t* m_playbackHandle{nullptr};

    // Configuration
    std::string m_deviceName;
    int m_sampleRate{48000};
    int m_periodSize{128};
    int m_bufferSize{256};
    int m_numChannels{2}; // Stereo by default

    // Audio thread
    std::thread m_audioThread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_shouldStop{false};

    // Buffers (pre-allocated)
    int16_t* m_inputBufferS16{nullptr};
    int16_t* m_outputBufferS16{nullptr};
    float* m_inputBufferFloat{nullptr};
    float* m_outputBufferFloat{nullptr};

    // Monitoring
    std::atomic<uint32_t> m_xrunCount{0};

    // Real-time thread priority
    static constexpr int RT_PRIORITY = 90;
};

} // namespace HoopiPi
