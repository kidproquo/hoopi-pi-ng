#pragma once

#include <jack/jack.h>
#include <string>
#include <atomic>
#include <mutex>

namespace HoopiPi {

class Engine;
class BackingTrack;

/**
 * @brief JACK connection status
 */
enum class JackStatus {
    Disconnected,  // Not connected to JACK
    Connecting,    // Attempting to connect
    Connected,     // Successfully connected and running
    Error          // Connection error
};

/**
 * @brief JACK audio backend for real-time processing
 *
 * JACK handles all timing and synchronization automatically,
 * making it much more reliable than direct ALSA for low-latency audio.
 */
class JackBackend {
public:
    /**
     * @brief Construct JACK backend
     * @param engine Pointer to processing engine
     */
    explicit JackBackend(Engine* engine);

    /**
     * @brief Destructor - ensures clean shutdown
     */
    ~JackBackend();

    /**
     * @brief Initialize JACK client and ports
     * @param clientName JACK client name (default: "HoopiPi")
     * @param autoConnect Auto-connect to system ports
     * @return true if successful
     */
    bool init(const std::string& clientName = "HoopiPi", bool autoConnect = true);

    /**
     * @brief Activate JACK client (start processing)
     * @return true if successful
     */
    bool start();

    /**
     * @brief Deactivate JACK client (stop processing)
     */
    void stop();

    /**
     * @brief Check if backend is running
     * @return true if JACK client is active
     */
    bool isRunning() const { return m_running.load(std::memory_order_acquire); }

    /**
     * @brief Get JACK connection status
     * @return Current connection status
     */
    JackStatus getStatus() const { return m_status.load(std::memory_order_acquire); }

    /**
     * @brief Get last error message
     * @return Error message string (empty if no error)
     */
    std::string getErrorMessage() const;

    /**
     * @brief Attempt to reconnect to JACK
     * @param clientName JACK client name
     * @param autoConnect Auto-connect to system ports
     * @return true if reconnection successful
     */
    bool reconnect(const std::string& clientName = "HoopiPi", bool autoConnect = true);

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
     * @brief Get actual sample rate from JACK
     * @return Sample rate in Hz
     */
    int getSampleRate() const;

    /**
     * @brief Get buffer size from JACK
     * @return Buffer size in frames
     */
    int getBufferSize() const;

    /**
     * @brief Get latency in milliseconds
     * @return Latency in ms
     */
    float getLatencyMs() const;

    /**
     * @brief Get CPU load percentage from JACK (system-wide audio DSP load)
     * @return CPU load as percentage (0-100)
     */
    float getCPULoad() const;

    /**
     * @brief Get HoopiPi process CPU usage
     * @return CPU usage percentage (0-100), or -1.0 if unavailable
     */
    float getProcessCPUUsage() const;

    /**
     * @brief Get CPU temperature
     * @return CPU temperature in degrees Celsius, or -1.0 if unavailable
     */
    float getCPUTemperature() const;

    /**
     * @brief Get memory usage
     * @return Memory usage in MB, or -1.0 if unavailable
     */
    float getMemoryUsage() const;

    /**
     * @brief Set backing track for mixing
     * @param backingTrack Pointer to backing track (can be nullptr)
     */
    void setBackingTrack(BackingTrack* backingTrack);

private:
    // JACK callbacks (must be static)
    static int processCallback(jack_nframes_t nframes, void* arg);
    static int bufferSizeCallback(jack_nframes_t nframes, void* arg);
    static int sampleRateCallback(jack_nframes_t nframes, void* arg);
    static int xrunCallback(void* arg);
    static void shutdownCallback(void* arg);

    // Internal processing
    int process(jack_nframes_t nframes);

    // Engine
    Engine* m_engine;

    // Backing track
    BackingTrack* m_backingTrack{nullptr};

    // JACK client and ports
    jack_client_t* m_client{nullptr};
    jack_port_t* m_inputPortL{nullptr};
    jack_port_t* m_inputPortR{nullptr};
    jack_port_t* m_outputPortL{nullptr};
    jack_port_t* m_outputPortR{nullptr};

    // State
    std::atomic<bool> m_running{false};
    std::atomic<uint32_t> m_xrunCount{0};
    std::atomic<JackStatus> m_status{JackStatus::Disconnected};
    std::atomic<bool> m_shutdownCalled{false};  // Set by JACK shutdown callback
    std::string m_errorMessage;
    mutable std::mutex m_errorMutex;

    // Auto-connect flag
    bool m_autoConnect{true};
    std::string m_clientName{"HoopiPi"};

    // Process CPU tracking
    mutable unsigned long long m_lastTotalTime{0};
    mutable unsigned long long m_lastProcessTime{0};
};

} // namespace HoopiPi
