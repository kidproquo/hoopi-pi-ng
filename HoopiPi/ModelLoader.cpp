#include "ModelLoader.h"
#include "NeuralModel.h"
#include <chrono>
#include <cstring>
#include <cmath>

namespace HoopiPi {

ModelLoader::ModelLoader(int sampleRate, int maxBufferSize)
    : m_sampleRate(sampleRate)
    , m_maxBufferSize(maxBufferSize)
{
#ifdef BUILD_STATIC_RTNEURAL
    // Configure to use static RTNeural models for optimal performance
    NeuralAudio::NeuralModel::SetWaveNetLoadMode(NeuralAudio::EModelLoadMode::RTNeural);
    NeuralAudio::NeuralModel::SetLSTMLoadMode(NeuralAudio::EModelLoadMode::RTNeural);
#endif

    // Start worker thread
    m_workerRunning.store(true, std::memory_order_release);
    m_workerThread = std::thread(&ModelLoader::workerThread, this);
}

ModelLoader::~ModelLoader() {
    // Stop worker thread
    m_workerRunning.store(false, std::memory_order_release);
    m_workerCV.notify_all();

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    // Clean up model
    m_model.reset();
}

void ModelLoader::loadModelAsync(const std::string& modelPath) {
    {
        std::lock_guard<std::mutex> lock(m_workerMutex);
        m_pendingModelPath = modelPath;
        m_loadRequested.store(true, std::memory_order_release);
    }
    m_workerCV.notify_one();
}

void ModelLoader::unloadModel() {
    // Trigger fade out
    startFadeOut();

    // Wait for fade out to complete (with timeout)
    std::unique_lock<std::mutex> lock(m_syncMutex);
    m_syncCV.wait_for(lock, std::chrono::milliseconds(100), [this] {
        return m_fadeState == FadeState::Idle;
    });

    // Clear model
    {
        std::lock_guard<std::mutex> modelLock(m_modelMutex);
        m_model.reset();
    }

    m_ready.store(false, std::memory_order_release);
    m_modelPath.clear();
}

std::string ModelLoader::getModelPath() const {
    return m_modelPath;
}

void ModelLoader::process(const float* input, float* output, size_t numSamples, bool applyNormalization) {
    // Check if we have a model
    if (!m_ready.load(std::memory_order_acquire)) {
        // No model - copy input to output (bypass)
        std::memcpy(output, input, numSamples * sizeof(float));
        return;
    }

    // Process through model
    {
        std::lock_guard<std::mutex> lock(m_modelMutex);
        if (m_model) {
            // NeuralModel::Process expects non-const input (modifies in-place for some models)
            // We need to const_cast here since we know the input won't be modified for most models
            m_model->Process(const_cast<float*>(input), output, numSamples);
        } else {
            std::memcpy(output, input, numSamples * sizeof(float));
            return;
        }
    }

    // Apply normalization gain if requested
    if (applyNormalization) {
        float normGain = m_normalizationGain.load(std::memory_order_relaxed);
        if (normGain != 1.0f) {
            for (size_t i = 0; i < numSamples; ++i) {
                output[i] *= normGain;
            }
        }
    }

    // Apply fade envelope
    applyFade(output, numSamples);
}

float ModelLoader::getRecommendedInputGain() const {
    std::lock_guard<std::mutex> lock(m_modelMutex);
    if (m_model) {
        return m_model->GetRecommendedInputDBAdjustment();
    }
    return 0.0f;
}

float ModelLoader::getRecommendedOutputGain() const {
    std::lock_guard<std::mutex> lock(m_modelMutex);
    if (m_model) {
        return m_model->GetRecommendedOutputDBAdjustment();
    }
    return 0.0f;
}

int ModelLoader::getLoadMode() const {
    std::lock_guard<std::mutex> lock(m_modelMutex);
    if (m_model) {
        return static_cast<int>(m_model->GetLoadMode());
    }
    return 0;
}

bool ModelLoader::isStatic() const {
    std::lock_guard<std::mutex> lock(m_modelMutex);
    if (m_model) {
        return m_model->IsStatic();
    }
    return false;
}

// ===== Worker Thread =====

void ModelLoader::workerThread() {
    while (m_workerRunning.load(std::memory_order_acquire)) {
        // Wait for work
        std::unique_lock<std::mutex> lock(m_workerMutex);
        m_workerCV.wait(lock, [this] {
            return m_loadRequested.load(std::memory_order_acquire) ||
                   !m_workerRunning.load(std::memory_order_acquire);
        });

        if (!m_workerRunning.load(std::memory_order_acquire)) {
            break;
        }

        if (m_loadRequested.load(std::memory_order_acquire)) {
            std::string pathToLoad = m_pendingModelPath;
            m_loadRequested.store(false, std::memory_order_release);
            lock.unlock();

            // Load the model
            doLoadModel(pathToLoad);
        }
    }
}

void ModelLoader::doLoadModel(const std::string& path) {
    // Fade out current model if any
    if (m_ready.load(std::memory_order_acquire)) {
        startFadeOut();

        // Wait for fade out (with timeout)
        std::unique_lock<std::mutex> lock(m_syncMutex);
        m_syncCV.wait_for(lock, std::chrono::milliseconds(60), [this] {
            return m_fadeState == FadeState::Idle;
        });
    }

    // Mark not ready during loading
    m_ready.store(false, std::memory_order_release);

    try {
        // Load the model
        std::unique_ptr<NeuralAudio::NeuralModel> newModel(
            NeuralAudio::NeuralModel::CreateFromFile(path.c_str())
        );

        if (!newModel) {
            throw std::runtime_error("Failed to load model");
        }

        // Set max buffer size
        newModel->SetMaxAudioBufferSize(m_maxBufferSize);

        // Get model sample rate
        int modelSR = static_cast<int>(newModel->GetSampleRate());
        m_modelSampleRate.store(modelSR, std::memory_order_release);

        // Calculate normalization gain (based on NeuralRack)
        float loudness = newModel->GetRecommendedOutputDBAdjustment();
        float normGain = std::pow(10.0f, (-6.0f + loudness) / 20.0f);
        m_normalizationGain.store(normGain, std::memory_order_release);

        // Prewarm the model (fill internal buffers)
        constexpr size_t PREWARM_SAMPLES = 256;
        float prewarmBuffer[PREWARM_SAMPLES] = {0};
        newModel->Process(prewarmBuffer, prewarmBuffer, PREWARM_SAMPLES);

        // Swap in the new model
        {
            std::lock_guard<std::mutex> lock(m_modelMutex);
            m_model = std::move(newModel);
        }

        m_modelPath = path;

        // Start fade in
        startFadeIn();

        // Mark ready
        m_ready.store(true, std::memory_order_release);

    } catch (const std::exception& e) {
        // Model loading failed
        m_ready.store(false, std::memory_order_release);
        m_modelPath.clear();

        // TODO: Call error callback if set
        // For now, just continue
    }
}

// ===== Fade Handling =====

void ModelLoader::startFadeOut() {
    m_doRampDown.store(true, std::memory_order_release);
    m_fadeState = FadeState::FadingOut;
    m_fadeSamplesRemaining = FADE_SAMPLES;
    m_fadeGain = 1.0f;
}

void ModelLoader::startFadeIn() {
    m_doRampUp.store(true, std::memory_order_release);
    m_fadeState = FadeState::FadingIn;
    m_fadeSamplesRemaining = FADE_SAMPLES;
    m_fadeGain = 0.0f;
}

void ModelLoader::applyFade(float* buffer, size_t numSamples) {
    if (m_fadeState == FadeState::Idle) {
        return; // No fading
    }

    for (size_t i = 0; i < numSamples; ++i) {
        if (m_fadeSamplesRemaining > 0) {
            if (m_fadeState == FadeState::FadingOut) {
                // Fade out (linear)
                m_fadeGain = static_cast<float>(m_fadeSamplesRemaining) / FADE_SAMPLES;
            } else if (m_fadeState == FadeState::FadingIn) {
                // Fade in (linear)
                m_fadeGain = 1.0f - (static_cast<float>(m_fadeSamplesRemaining) / FADE_SAMPLES);
            }

            buffer[i] *= m_fadeGain;
            m_fadeSamplesRemaining--;

            if (m_fadeSamplesRemaining == 0) {
                // Fade complete
                if (m_fadeState == FadeState::FadingOut) {
                    m_doRampDown.store(false, std::memory_order_release);
                    m_fadeGain = 0.0f;

                    // Notify waiting threads
                    m_syncCV.notify_all();
                } else if (m_fadeState == FadeState::FadingIn) {
                    m_doRampUp.store(false, std::memory_order_release);
                    m_fadeGain = 1.0f;
                }

                m_fadeState = FadeState::Idle;
            }
        }
    }
}

} // namespace HoopiPi
