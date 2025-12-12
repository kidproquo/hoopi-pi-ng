#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace NeuralAudio {
    class NeuralModel;
}

namespace HoopiPi {

/**
 * @brief Asynchronous model loader with fade-in/fade-out
 *
 * Based on NeuralRack's NeuralModelLoader, handles loading models
 * in a background thread with smooth transitions.
 */
class ModelLoader {
public:
    ModelLoader(int sampleRate, int maxBufferSize);
    ~ModelLoader();

    // ===== Model Loading =====

    /**
     * @brief Load a model file asynchronously
     * @param modelPath Path to .nam or .json model file
     *
     * This triggers loading in the background worker thread.
     * The current model (if any) will fade out, then the new model loads.
     */
    void loadModelAsync(const std::string& modelPath);

    /**
     * @brief Unload current model
     */
    void unloadModel();

    /**
     * @brief Check if model is ready for processing
     * @return true if model loaded and ready
     */
    bool isReady() const { return m_ready.load(std::memory_order_acquire); }

    /**
     * @brief Get current model file path
     * @return Path to loaded model, or empty string if none
     */
    std::string getModelPath() const;

    // ===== Processing =====

    /**
     * @brief Process audio through the model (real-time safe)
     * @param input Input buffer
     * @param output Output buffer
     * @param numSamples Number of samples to process
     * @param applyNormalization Apply model's recommended output gain
     *
     * Handles fade-in/fade-out smoothly during model transitions.
     */
    void process(const float* input, float* output, size_t numSamples, bool applyNormalization);

    // ===== Model Info =====

    /**
     * @brief Get model's native sample rate
     * @return Sample rate in Hz, or 0 if no model loaded
     */
    int getModelSampleRate() const { return m_modelSampleRate.load(std::memory_order_acquire); }

    /**
     * @brief Get recommended input gain adjustment
     * @return Gain adjustment in dB
     */
    float getRecommendedInputGain() const;

    /**
     * @brief Get recommended output gain adjustment
     * @return Gain adjustment in dB
     */
    float getRecommendedOutputGain() const;

    /**
     * @brief Get model load mode (Internal/RTNeural/NAMCore)
     * @return Load mode enum value
     */
    int getLoadMode() const;

    /**
     * @brief Check if model uses static (template) implementation
     * @return true if static model
     */
    bool isStatic() const;

private:
    // Worker thread for model loading
    void workerThread();
    void doLoadModel(const std::string& path);

    // Fade handling
    void startFadeOut();
    void startFadeIn();
    void applyFade(float* buffer, size_t numSamples);

    // Configuration
    int m_sampleRate;
    int m_maxBufferSize;

    // Model state
    std::unique_ptr<NeuralAudio::NeuralModel> m_model;
    std::string m_modelPath;
    mutable std::mutex m_modelMutex; // Protects m_model pointer swap (mutable for const methods)

    // Atomic state flags
    std::atomic<bool> m_ready{false};
    std::atomic<bool> m_doRampDown{false};
    std::atomic<bool> m_doRampUp{false};
    std::atomic<int> m_modelSampleRate{0};

    // Fade state
    float m_fadeGain{0.0f};
    static constexpr int FADE_SAMPLES = 256; // ~5ms @ 48kHz
    int m_fadeSamplesRemaining{0};
    enum class FadeState { Idle, FadingOut, FadingIn };
    FadeState m_fadeState{FadeState::Idle};

    // Normalization gain
    std::atomic<float> m_normalizationGain{1.0f};

    // Worker thread
    std::thread m_workerThread;
    std::atomic<bool> m_workerRunning{false};
    std::atomic<bool> m_loadRequested{false};
    std::string m_pendingModelPath;
    std::mutex m_workerMutex;
    std::condition_variable m_workerCV;

    // Sync for fade-out completion
    std::mutex m_syncMutex;
    std::condition_variable m_syncCV;
};

} // namespace HoopiPi
