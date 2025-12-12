#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <functional>

namespace NeuralAudio {
    class NeuralModel;
}

namespace HoopiPi {

class ModelLoader;
class AlsaBackend;
class NoiseGate;
class DCBlocker;
class ThreeBandEQ;
class Reverb;
class AudioRecorder;

/**
 * @brief Stereo processing modes
 */
enum class StereoMode {
    LeftMono2Stereo,   ///< Process left input only, output to both L/R
    Stereo2Stereo,     ///< Process left and right inputs independently
    RightMono2Stereo,  ///< Process right input only, output to both L/R
    Stereo2Mono        ///< Mix left and right inputs, process as mono, output to both L/R
};

/**
 * @brief Main audio processing engine for HoopiPi
 *
 * Manages real-time audio processing with neural models, following
 * NeuralRack's architecture with async model loading and lock-free
 * parameter updates.
 */
class Engine {
public:
    /**
     * @brief Construct engine with specified sample rate and buffer size
     * @param sampleRate Sample rate in Hz (typically 48000)
     * @param bufferSize Maximum buffer size in samples (typically 128)
     */
    Engine(int sampleRate, int bufferSize);

    /**
     * @brief Destructor - ensures clean shutdown
     */
    ~Engine();

    // ===== Lifecycle =====

    /**
     * @brief Initialize the engine and allocate resources
     * @return true if initialization successful
     */
    bool init();

    /**
     * @brief Cleanup and release resources
     */
    void cleanup();

    // ===== Audio Processing =====

    /**
     * @brief Process audio buffer (real-time safe)
     * @param input Input audio buffer (mono)
     * @param output Output audio buffer (mono)
     * @param numSamples Number of samples to process
     *
     * This function is called from the real-time audio thread and must
     * complete within the buffer deadline. No allocations or blocking calls.
     */
    void process(const float* input, float* output, size_t numSamples);

    /**
     * @brief Process stereo audio (real-time safe)
     * @param inputL Left input buffer
     * @param inputR Right input buffer (can be nullptr for mono)
     * @param outputL Left output buffer
     * @param outputR Right output buffer
     * @param numSamples Number of samples to process
     */
    void processStereo(const float* inputL, const float* inputR,
                       float* outputL, float* outputR, size_t numSamples);

    // ===== Model Management =====

    /**
     * @brief Load a model asynchronously (non-blocking)
     * @param slot Model slot (0 or 1 for A/B switching)
     * @param modelPath Path to .nam or .json model file
     *
     * Model loading happens in background thread. Use IsModelReady() to check
     * when loading is complete, or set a callback with SetModelLoadCallback().
     */
    void loadModelAsync(int slot, const std::string& modelPath);

    /**
     * @brief Check if model is loaded and ready
     * @param slot Model slot (0 or 1)
     * @return true if model is ready for processing
     */
    bool isModelReady(int slot) const;

    /**
     * @brief Set which model slot is active for processing
     * @param slot Model slot (0 or 1)
     */
    void setActiveModel(int slot);

    /**
     * @brief Get current active model slot
     * @return Active slot number (0 or 1)
     */
    int getActiveModel() const;

    /**
     * @brief Get model path for a slot
     * @param slot Model slot (0 or 1)
     * @return Model path or empty string if no model loaded
     */
    std::string getModelPath(int slot) const;

    /**
     * @brief Unload a model from slot
     * @param slot Model slot to unload
     */
    void unloadModel(int slot);

    // ===== Stereo Mode =====

    /**
     * @brief Set stereo processing mode
     * @param mode Stereo mode (LeftMono2Stereo, Stereo2Stereo, RightMono2Stereo)
     */
    void setStereoMode(StereoMode mode);

    /**
     * @brief Get current stereo mode
     * @return Current stereo mode
     */
    StereoMode getStereoMode() const;

    /**
     * @brief Set mix level for left channel in Stereo2Mono mode
     * @param level Mix level (0.0 to 1.0, default 0.5 for 50%)
     */
    void setStereo2MonoMixL(float level);

    /**
     * @brief Set mix level for right channel in Stereo2Mono mode
     * @param level Mix level (0.0 to 1.0, default 0.5 for 50%)
     */
    void setStereo2MonoMixR(float level);

    /**
     * @brief Get mix level for left channel in Stereo2Mono mode
     * @return Mix level (0.0 to 1.0)
     */
    float getStereo2MonoMixL() const;

    /**
     * @brief Get mix level for right channel in Stereo2Mono mode
     * @return Mix level (0.0 to 1.0)
     */
    float getStereo2MonoMixR() const;

    // ===== Per-Channel Model Slots =====

    /**
     * @brief Set active model slot for left channel
     * @param slot Model slot (0 or 1)
     */
    void setActiveModelL(int slot);

    /**
     * @brief Set active model slot for right channel
     * @param slot Model slot (0 or 1)
     */
    void setActiveModelR(int slot);

    /**
     * @brief Get active model slot for left channel
     * @return Active slot number (0 or 1)
     */
    int getActiveModelL() const;

    /**
     * @brief Get active model slot for right channel
     * @return Active slot number (0 or 1)
     */
    int getActiveModelR() const;

    /**
     * @brief Enable/disable NAM model bypass for left channel
     * @param bypass true to bypass NAM model for left channel
     */
    void setBypassModelL(bool bypass);

    /**
     * @brief Enable/disable NAM model bypass for right channel
     * @param bypass true to bypass NAM model for right channel
     */
    void setBypassModelR(bool bypass);

    /**
     * @brief Get NAM model bypass state for left channel
     * @return true if left channel NAM is bypassed
     */
    bool getBypassModelL() const;

    /**
     * @brief Get NAM model bypass state for right channel
     * @return true if right channel NAM is bypassed
     */
    bool getBypassModelR() const;

    // ===== Parameters (all real-time safe) =====

    /**
     * @brief Set input gain in dB (applies to both channels)
     * @param gainDB Gain in decibels (-inf to +20dB typical)
     */
    void setInputGain(float gainDB);

    /**
     * @brief Set left channel input gain in dB
     * @param gainDB Gain in decibels
     */
    void setInputGainL(float gainDB);

    /**
     * @brief Set right channel input gain in dB
     * @param gainDB Gain in decibels
     */
    void setInputGainR(float gainDB);

    /**
     * @brief Set output gain in dB (applies to both channels)
     * @param gainDB Gain in decibels
     */
    void setOutputGain(float gainDB);

    /**
     * @brief Set left channel output gain in dB
     * @param gainDB Gain in decibels
     */
    void setOutputGainL(float gainDB);

    /**
     * @brief Set right channel output gain in dB
     * @param gainDB Gain in decibels
     */
    void setOutputGainR(float gainDB);

    /**
     * @brief Enable/disable bypass
     * @param bypass true to bypass processing (pass-through)
     */
    void setBypass(bool bypass);

    /**
     * @brief Enable/disable NAM model bypass
     * @param bypass true to bypass NAM model (signal chain still active)
     */
    void setBypassModel(bool bypass);

    /**
     * @brief Enable/disable automatic output normalization
     * @param normalize true to apply model's recommended output gain
     */
    void setNormalize(bool normalize);

    /**
     * @brief Configure noise gate (both channels)
     * @param enabled Enable/disable gate
     * @param thresholdDB Threshold in dB (-60 to 0 typical)
     */
    void setNoiseGate(bool enabled, float thresholdDB);

    /**
     * @brief Configure left channel noise gate
     * @param enabled Enable/disable gate
     * @param thresholdDB Threshold in dB
     */
    void setNoiseGateL(bool enabled, float thresholdDB);

    /**
     * @brief Configure right channel noise gate
     * @param enabled Enable/disable gate
     * @param thresholdDB Threshold in dB
     */
    void setNoiseGateR(bool enabled, float thresholdDB);

    /**
     * @brief Enable/disable DC blocking filter
     * @param enabled true to enable DC blocker
     */
    void setDCBlocker(bool enabled);

    /**
     * @brief Configure EQ (both channels)
     * @param enabled Enable/disable EQ
     */
    void setEQEnabled(bool enabled);

    /**
     * @brief Set EQ bass gain in dB (both channels)
     * @param gainDB Gain in decibels (-20 to +20 dB)
     */
    void setEQBass(float gainDB);

    /**
     * @brief Set EQ mid gain in dB (both channels)
     * @param gainDB Gain in decibels (-20 to +20 dB)
     */
    void setEQMid(float gainDB);

    /**
     * @brief Set EQ treble gain in dB (both channels)
     * @param gainDB Gain in decibels (-20 to +20 dB)
     */
    void setEQTreble(float gainDB);

    /**
     * @brief Configure left channel EQ
     * @param enabled Enable/disable EQ
     */
    void setEQEnabledL(bool enabled);

    /**
     * @brief Set left channel EQ bass gain in dB
     * @param gainDB Gain in decibels (-20 to +20 dB)
     */
    void setEQBassL(float gainDB);

    /**
     * @brief Set left channel EQ mid gain in dB
     * @param gainDB Gain in decibels (-20 to +20 dB)
     */
    void setEQMidL(float gainDB);

    /**
     * @brief Set left channel EQ treble gain in dB
     * @param gainDB Gain in decibels (-20 to +20 dB)
     */
    void setEQTrebleL(float gainDB);

    /**
     * @brief Configure right channel EQ
     * @param enabled Enable/disable EQ
     */
    void setEQEnabledR(bool enabled);

    /**
     * @brief Set right channel EQ bass gain in dB
     * @param gainDB Gain in decibels (-20 to +20 dB)
     */
    void setEQBassR(float gainDB);

    /**
     * @brief Set right channel EQ mid gain in dB
     * @param gainDB Gain in decibels (-20 to +20 dB)
     */
    void setEQMidR(float gainDB);

    /**
     * @brief Set right channel EQ treble gain in dB
     * @param gainDB Gain in decibels (-20 to +20 dB)
     */
    void setEQTrebleR(float gainDB);

    // ===== Reverb =====

    /**
     * @brief Enable/disable reverb
     * @param enabled true to enable reverb
     */
    void setReverbEnabled(bool enabled);

    /**
     * @brief Set reverb room size
     * @param size Room size (0.0 = small room, 1.0 = large hall)
     */
    void setReverbRoomSize(float size);

    /**
     * @brief Set reverb decay time
     * @param seconds RT60 decay time in seconds (0.1-10.0)
     */
    void setReverbDecayTime(float seconds);

    /**
     * @brief Set reverb dry/wet mix
     * @param dry Dry signal level (0.0-1.0)
     * @param wet Wet signal level (0.0-1.0)
     */
    void setReverbMix(float dry, float wet);

    /**
     * @brief Clear reverb buffers (removes reverb tail)
     */
    void clearReverbBuffers();

    bool getReverbEnabled() const;
    float getReverbRoomSize() const;
    float getReverbDecayTime() const;
    float getReverbDry() const;
    float getReverbWet() const;

    // ===== Recording =====

    /**
     * @brief Start recording audio to WAV file
     * @param filename Optional filename (auto-generated if empty)
     * @return Full path to recording file, or empty string on error
     */
    std::string startRecording(const std::string& filename = "");

    /**
     * @brief Stop recording and finalize WAV file
     */
    void stopRecording();

    /**
     * @brief Check if currently recording
     */
    bool isRecording() const;

    /**
     * @brief Get current recording file path
     */
    std::string getRecordingFilePath() const;

    /**
     * @brief Get number of dropped frames during recording
     */
    uint64_t getRecordingDroppedFrames() const;

    /**
     * @brief Get recording duration in seconds
     * @return Duration in seconds, or 0.0 if not recording
     */
    double getRecordingDuration() const;

    /**
     * @brief Set backing track for optional mixing into recordings
     * @param backingTrack Pointer to BackingTrack instance (can be nullptr)
     */
    void setBackingTrack(class BackingTrack* backingTrack);

    /**
     * @brief Enable/disable mixing backing track into recordings
     * @param enabled true to include backing track in recordings
     */
    void setIncludeBackingTrackInRecording(bool enabled);

    /**
     * @brief Check if backing track will be included in recordings
     */
    bool getIncludeBackingTrackInRecording() const;

    // ===== Getters =====

    float getInputGain() const;
    float getInputGainL() const;
    float getInputGainR() const;
    float getOutputGain() const;
    float getOutputGainL() const;
    float getOutputGainR() const;
    bool getBypass() const;
    bool getBypassModel() const;
    bool getNormalize() const;
    bool getNoiseGateEnabled() const;
    bool getNoiseGateEnabledL() const;
    bool getNoiseGateEnabledR() const;
    float getNoiseGateThreshold() const;
    float getNoiseGateThresholdL() const;
    float getNoiseGateThresholdR() const;
    bool getDCBlockerEnabled() const;
    bool getEQEnabled() const;
    bool getEQEnabledL() const;
    bool getEQEnabledR() const;
    float getEQBass() const;
    float getEQBassL() const;
    float getEQBassR() const;
    float getEQMid() const;
    float getEQMidL() const;
    float getEQMidR() const;
    float getEQTreble() const;
    float getEQTrebleL() const;
    float getEQTrebleR() const;

    int getSampleRate() const { return m_sampleRate; }
    int getBufferSize() const { return m_bufferSize; }

    // ===== Monitoring =====

    /**
     * @brief Get total xrun (buffer underrun) count
     * @return Number of xruns since engine start
     */
    uint32_t getXrunCount() const;

    /**
     * @brief Reset xrun counter
     */
    void resetXrunCount();

    /**
     * @brief Get processing latency
     * @return Latency in samples
     */
    float getLatency() const;

    // ===== Callbacks =====

    using ModelLoadCallback = std::function<void(int slot, bool success, const std::string& error)>;

    /**
     * @brief Set callback for model load completion
     * @param callback Function called when model loading completes
     */
    void setModelLoadCallback(ModelLoadCallback callback);

private:
    // Configuration
    int m_sampleRate;
    int m_bufferSize;

    // Stereo mode
    std::atomic<StereoMode> m_stereoMode{StereoMode::LeftMono2Stereo};

    // Stereo2Mono mix levels (0.0 to 1.0, default 0.5 for 50/50 mix)
    std::atomic<float> m_stereo2MonoMixL{0.5f};
    std::atomic<float> m_stereo2MonoMixR{0.5f};

    // Model slots (A/B)
    std::unique_ptr<ModelLoader> m_modelSlots[2];
    std::atomic<int> m_activeSlot{0};  // Legacy: both channels use same slot
    std::atomic<int> m_activeSlotL{0};  // Per-channel: left channel active slot
    std::atomic<int> m_activeSlotR{0};  // Per-channel: right channel active slot
    std::string m_modelPaths[2];  // Track loaded model paths

    // Processing modules - left channel
    std::unique_ptr<NoiseGate> m_noiseGateL;
    std::unique_ptr<DCBlocker> m_dcBlockerL;
    std::unique_ptr<ThreeBandEQ> m_eqL;

    // Processing modules - right channel
    std::unique_ptr<NoiseGate> m_noiseGateR;
    std::unique_ptr<DCBlocker> m_dcBlockerR;
    std::unique_ptr<ThreeBandEQ> m_eqR;

    // Legacy processing modules (for backwards compatibility)
    std::unique_ptr<NoiseGate> m_noiseGate;
    std::unique_ptr<DCBlocker> m_dcBlocker;
    std::unique_ptr<ThreeBandEQ> m_eq;

    // Reverb (shared for stereo processing)
    std::unique_ptr<Reverb> m_reverb;

    std::unique_ptr<AudioRecorder> m_recorder;

    // Backing track for optional mixing into recordings
    class BackingTrack* m_backingTrack{nullptr};
    std::atomic<bool> m_includeBackingTrackInRecording{false};

    // Parameters (atomic for lock-free updates) - left channel
    std::atomic<float> m_inputGainLinearL{1.0f};
    std::atomic<float> m_outputGainLinearL{1.0f};
    std::atomic<bool> m_noiseGateEnabledL{false};

    // Parameters (atomic for lock-free updates) - right channel
    std::atomic<float> m_inputGainLinearR{1.0f};
    std::atomic<float> m_outputGainLinearR{1.0f};
    std::atomic<bool> m_noiseGateEnabledR{false};

    // Legacy parameters (for backwards compatibility)
    std::atomic<float> m_inputGainLinear{1.0f};
    std::atomic<float> m_outputGainLinear{1.0f};
    std::atomic<bool> m_bypass{false};
    std::atomic<bool> m_bypassModel{false};  // Legacy: both channels bypass together
    std::atomic<bool> m_bypassModelL{false};  // Per-channel: left channel bypass
    std::atomic<bool> m_bypassModelR{true};   // Per-channel: right channel bypass (default: bypassed for mic input)
    std::atomic<bool> m_normalize{true};
    std::atomic<bool> m_noiseGateEnabled{false};
    std::atomic<bool> m_dcBlockerEnabled{false};

    // Gain smoothing (to prevent clicks) - left channel
    float m_currentInputGainL{1.0f};
    float m_currentOutputGainL{1.0f};

    // Gain smoothing (to prevent clicks) - right channel
    float m_currentInputGainR{1.0f};
    float m_currentOutputGainR{1.0f};

    // Legacy gain smoothing (for backwards compatibility)
    float m_currentInputGain{1.0f};
    float m_currentOutputGain{1.0f};
    static constexpr float GAIN_SMOOTH_COEFF = 0.999f;

    // Monitoring
    std::atomic<uint32_t> m_xrunCount{0};

    // Callback
    ModelLoadCallback m_modelLoadCallback;

    // Internal buffers (pre-allocated)
    std::unique_ptr<float[]> m_processBuffer;
    std::unique_ptr<float[]> m_processBufferL;
    std::unique_ptr<float[]> m_processBufferR;

    // Helper functions
    void smoothGains();
    float dbToLinear(float db) const;
    float linearToDb(float linear) const;
};

} // namespace HoopiPi
