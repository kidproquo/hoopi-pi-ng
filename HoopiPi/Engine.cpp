#include "Engine.h"
#include "ModelLoader.h"
#include "NoiseGate.h"
#include "DCBlocker.h"
#include "ThreeBandEQ.h"
#include "Reverb.h"
#include "AudioRecorder.h"
#include "../standalone/BackingTrack.h"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace HoopiPi {

Engine::Engine(int sampleRate, int bufferSize)
    : m_sampleRate(sampleRate)
    , m_bufferSize(bufferSize)
{
    // Create model slots
    m_modelSlots[0] = std::make_unique<ModelLoader>(sampleRate, bufferSize);
    m_modelSlots[1] = std::make_unique<ModelLoader>(sampleRate, bufferSize);

    // Create processing modules - left channel
    m_noiseGateL = std::make_unique<NoiseGate>(sampleRate);
    m_dcBlockerL = std::make_unique<DCBlocker>(sampleRate);
    m_eqL = std::make_unique<ThreeBandEQ>(sampleRate);

    // Create processing modules - right channel
    m_noiseGateR = std::make_unique<NoiseGate>(sampleRate);
    m_dcBlockerR = std::make_unique<DCBlocker>(sampleRate);
    m_eqR = std::make_unique<ThreeBandEQ>(sampleRate);

    // Create legacy processing modules (backwards compatibility)
    m_noiseGate = std::make_unique<NoiseGate>(sampleRate);
    m_dcBlocker = std::make_unique<DCBlocker>(sampleRate);
    m_eq = std::make_unique<ThreeBandEQ>(sampleRate);

    // Create reverb (shared for stereo processing)
    m_reverb = std::make_unique<Reverb>(sampleRate, bufferSize);

    // Create recorder (recordings directory in build folder)
    m_recorder = std::make_unique<AudioRecorder>("./recordings");

    // Allocate process buffers
    m_processBuffer = std::make_unique<float[]>(bufferSize);
    m_processBufferL = std::make_unique<float[]>(bufferSize);
    m_processBufferR = std::make_unique<float[]>(bufferSize);
}

Engine::~Engine() {
    cleanup();
}

bool Engine::init() {
    // Reset all state
    m_xrunCount.store(0);

    // Initialize legacy gains
    m_currentInputGain = m_inputGainLinear.load();
    m_currentOutputGain = m_outputGainLinear.load();

    // Initialize per-channel gains
    m_currentInputGainL = m_inputGainLinearL.load();
    m_currentOutputGainL = m_outputGainLinearL.load();
    m_currentInputGainR = m_inputGainLinearR.load();
    m_currentOutputGainR = m_outputGainLinearR.load();

    return true;
}

void Engine::cleanup() {
    // Unload models
    unloadModel(0);
    unloadModel(1);
}

void Engine::process(const float* input, float* output, size_t numSamples) {
    if (numSamples > static_cast<size_t>(m_bufferSize)) {
        // Buffer overflow - this is a programming error
        m_xrunCount.fetch_add(1, std::memory_order_relaxed);
        // Copy input to output and return
        std::memcpy(output, input, numSamples * sizeof(float));
        return;
    }

    // Check bypass
    if (m_bypass.load(std::memory_order_relaxed)) {
        std::memcpy(output, input, numSamples * sizeof(float));
        return;
    }

    // Copy input to process buffer
    std::memcpy(m_processBuffer.get(), input, numSamples * sizeof(float));
    float* buf = m_processBuffer.get();

    // Smooth gain transitions
    smoothGains();

    // Apply input gain
    if (m_currentInputGain != 1.0f) {
        for (size_t i = 0; i < numSamples; ++i) {
            buf[i] *= m_currentInputGain;
        }
    }

    // Apply noise gate
    if (m_noiseGateEnabled.load(std::memory_order_relaxed)) {
        m_noiseGate->process(buf, numSamples);
    }

    // Process through active model
    int activeSlot = m_activeSlot.load(std::memory_order_acquire);
    bool bypassModel = m_bypassModel.load(std::memory_order_relaxed);
    if (!bypassModel && activeSlot >= 0 && activeSlot < 2 && m_modelSlots[activeSlot]->isReady()) {
        bool applyNorm = m_normalize.load(std::memory_order_relaxed);
        m_modelSlots[activeSlot]->process(buf, buf, numSamples, applyNorm);
    }

    // Apply EQ (after model, like NeuralRack)
    m_eq->process(buf, buf, numSamples);

    // Apply DC blocker
    if (m_dcBlockerEnabled.load(std::memory_order_relaxed)) {
        m_dcBlocker->process(buf, numSamples);
    }

    // Apply output gain
    if (m_currentOutputGain != 1.0f) {
        for (size_t i = 0; i < numSamples; ++i) {
            buf[i] *= m_currentOutputGain;
        }
    }

    // Push to recorder if recording (lock-free, ~50-100ns overhead)
    if (m_recorder && m_recorder->isRecording()) {
        m_recorder->pushAudioFrame(buf, buf, numSamples); // Stereo: same mono signal to both channels
    }

    // Copy to output
    std::memcpy(output, buf, numSamples * sizeof(float));
}

void Engine::processStereo(const float* inputL, const float* inputR,
                           float* outputL, float* outputR, size_t numSamples) {
    if (numSamples > static_cast<size_t>(m_bufferSize)) {
        // Buffer overflow - this is a programming error
        m_xrunCount.fetch_add(1, std::memory_order_relaxed);
        // Copy input to output and return
        std::memcpy(outputL, inputL, numSamples * sizeof(float));
        if (outputR && inputR) {
            std::memcpy(outputR, inputR, numSamples * sizeof(float));
        } else if (outputR) {
            std::memcpy(outputR, inputL, numSamples * sizeof(float));
        }
        return;
    }

    // Check bypass
    if (m_bypass.load(std::memory_order_relaxed)) {
        std::memcpy(outputL, inputL, numSamples * sizeof(float));
        if (outputR && inputR) {
            std::memcpy(outputR, inputR, numSamples * sizeof(float));
        } else if (outputR) {
            std::memcpy(outputR, inputL, numSamples * sizeof(float));
        }
        return;
    }

    // Get stereo mode
    StereoMode mode = m_stereoMode.load(std::memory_order_relaxed);

    // Smooth gain transitions
    smoothGains();

    // Select which input to use based on stereo mode
    const float* selectedInputL = inputL;
    const float* selectedInputR = (mode == StereoMode::Stereo2Stereo && inputR) ? inputR : inputL;

    if (mode == StereoMode::RightMono2Stereo && inputR) {
        selectedInputL = inputR;
        selectedInputR = inputR;
    }

    // Process left channel
    if (mode == StereoMode::Stereo2Mono && inputR) {
        // Mix L and R inputs with configurable levels
        float mixL = m_stereo2MonoMixL.load(std::memory_order_relaxed);
        float mixR = m_stereo2MonoMixR.load(std::memory_order_relaxed);
        float* bufL = m_processBufferL.get();
        for (size_t i = 0; i < numSamples; ++i) {
            bufL[i] = inputL[i] * mixL + inputR[i] * mixR;
        }
    } else {
        std::memcpy(m_processBufferL.get(), selectedInputL, numSamples * sizeof(float));
    }
    float* bufL = m_processBufferL.get();

    // Apply left input gain
    if (m_currentInputGainL != 1.0f) {
        for (size_t i = 0; i < numSamples; ++i) {
            bufL[i] *= m_currentInputGainL;
        }
    }

    // Apply left noise gate
    if (m_noiseGateEnabledL.load(std::memory_order_relaxed)) {
        m_noiseGateL->process(bufL, numSamples);
    }

    // Process through active model (left channel)
    int activeSlotL = m_activeSlotL.load(std::memory_order_acquire);
    bool bypassModelL = m_bypassModelL.load(std::memory_order_relaxed);
    if (!bypassModelL && activeSlotL >= 0 && activeSlotL < 2 && m_modelSlots[activeSlotL]->isReady()) {
        bool applyNorm = m_normalize.load(std::memory_order_relaxed);
        m_modelSlots[activeSlotL]->process(bufL, bufL, numSamples, applyNorm);
    }

    // Apply left EQ
    m_eqL->process(bufL, bufL, numSamples);

    // Apply left DC blocker
    if (m_dcBlockerEnabled.load(std::memory_order_relaxed)) {
        m_dcBlockerL->process(bufL, numSamples);
    }

    // Apply left output gain
    if (m_currentOutputGainL != 1.0f) {
        for (size_t i = 0; i < numSamples; ++i) {
            bufL[i] *= m_currentOutputGainL;
        }
    }

    // Process right channel
    float* bufR = m_processBufferR.get();

    if (mode == StereoMode::Stereo2Stereo) {
        // True stereo: L=guitar (with NAM), R=mic (no NAM)
        // Process right channel independently but SKIP NAM
        std::memcpy(bufR, selectedInputR, numSamples * sizeof(float));

        // Apply right input gain
        if (m_currentInputGainR != 1.0f) {
            for (size_t i = 0; i < numSamples; ++i) {
                bufR[i] *= m_currentInputGainR;
            }
        }

        // Apply right noise gate
        if (m_noiseGateEnabledR.load(std::memory_order_relaxed)) {
            m_noiseGateR->process(bufR, numSamples);
        }

        // SKIP NAM for right channel (it's a mic, not guitar)
        // Left channel already has NAM applied

        // Apply right EQ
        m_eqR->process(bufR, bufR, numSamples);

        // Apply right DC blocker
        if (m_dcBlockerEnabled.load(std::memory_order_relaxed)) {
            m_dcBlockerR->process(bufR, numSamples);
        }

        // Apply right output gain
        if (m_currentOutputGainR != 1.0f) {
            for (size_t i = 0; i < numSamples; ++i) {
                bufR[i] *= m_currentOutputGainR;
            }
        }
    } else {
        // Mono modes: copy left channel result to right (model already processed)
        std::memcpy(bufR, bufL, numSamples * sizeof(float));
    }

    // Apply reverb (shared for stereo processing)
    if (m_reverb && m_reverb->getEnabled()) {
        m_reverb->process(bufL, bufR, bufL, bufR, numSamples);
    }

    // Mix backing track into recording if enabled
    if (m_recorder && m_recorder->isRecording() &&
        m_backingTrack && m_includeBackingTrackInRecording.load() && m_backingTrack->isPlaying()) {

        // Allocate temporary buffers for backing track audio
        float trackBufferL[numSamples];
        float trackBufferR[numSamples];

        // Get backing track audio
        m_backingTrack->fillBuffer(trackBufferL, trackBufferR, numSamples);

        // Mix backing track into the audio buffers before recording
        for (size_t i = 0; i < numSamples; i++) {
            bufL[i] += trackBufferL[i];
            bufR[i] += trackBufferR[i];
        }

        // Now record with backing track mixed in
        m_recorder->pushAudioFrame(bufL, bufR, numSamples);
    } else if (m_recorder && m_recorder->isRecording()) {
        // Record without backing track
        m_recorder->pushAudioFrame(bufL, bufR, numSamples);
    }

    // Copy to outputs
    std::memcpy(outputL, bufL, numSamples * sizeof(float));
    if (outputR) {
        std::memcpy(outputR, bufR, numSamples * sizeof(float));
    }
}

void Engine::loadModelAsync(int slot, const std::string& modelPath) {
    if (slot < 0 || slot >= 2) return;

    m_modelSlots[slot]->loadModelAsync(modelPath);
    m_modelPaths[slot] = modelPath;
}

bool Engine::isModelReady(int slot) const {
    if (slot < 0 || slot >= 2) return false;
    return m_modelSlots[slot]->isReady();
}

void Engine::setActiveModel(int slot) {
    if (slot >= 0 && slot < 2) {
        m_activeSlot.store(slot, std::memory_order_release);
        // Also update per-channel slots to keep them in sync
        m_activeSlotL.store(slot, std::memory_order_release);
        m_activeSlotR.store(slot, std::memory_order_release);
    }
}

int Engine::getActiveModel() const {
    return m_activeSlot.load(std::memory_order_acquire);
}

void Engine::setActiveModelL(int slot) {
    if (slot >= 0 && slot < 2) {
        m_activeSlotL.store(slot, std::memory_order_release);
    }
}

void Engine::setActiveModelR(int slot) {
    if (slot >= 0 && slot < 2) {
        m_activeSlotR.store(slot, std::memory_order_release);
    }
}

int Engine::getActiveModelL() const {
    return m_activeSlotL.load(std::memory_order_acquire);
}

int Engine::getActiveModelR() const {
    return m_activeSlotR.load(std::memory_order_acquire);
}

void Engine::setBypassModelL(bool bypass) {
    m_bypassModelL.store(bypass, std::memory_order_relaxed);
}

void Engine::setBypassModelR(bool bypass) {
    m_bypassModelR.store(bypass, std::memory_order_relaxed);
}

bool Engine::getBypassModelL() const {
    return m_bypassModelL.load(std::memory_order_relaxed);
}

bool Engine::getBypassModelR() const {
    return m_bypassModelR.load(std::memory_order_relaxed);
}

std::string Engine::getModelPath(int slot) const {
    if (slot < 0 || slot >= 2) return "";
    return m_modelPaths[slot];
}

void Engine::unloadModel(int slot) {
    if (slot < 0 || slot >= 2) return;
    m_modelSlots[slot]->unloadModel();
    m_modelPaths[slot] = "";
}

// ===== Stereo Mode =====

void Engine::setStereoMode(StereoMode mode) {
    m_stereoMode.store(mode, std::memory_order_relaxed);
}

StereoMode Engine::getStereoMode() const {
    return m_stereoMode.load(std::memory_order_relaxed);
}

void Engine::setStereo2MonoMixL(float level) {
    // Clamp to 0.0-1.0 range
    float clampedLevel = std::max(0.0f, std::min(1.0f, level));
    m_stereo2MonoMixL.store(clampedLevel, std::memory_order_relaxed);
}

void Engine::setStereo2MonoMixR(float level) {
    // Clamp to 0.0-1.0 range
    float clampedLevel = std::max(0.0f, std::min(1.0f, level));
    m_stereo2MonoMixR.store(clampedLevel, std::memory_order_relaxed);
}

float Engine::getStereo2MonoMixL() const {
    return m_stereo2MonoMixL.load(std::memory_order_relaxed);
}

float Engine::getStereo2MonoMixR() const {
    return m_stereo2MonoMixR.load(std::memory_order_relaxed);
}

// ===== Parameters =====

void Engine::setInputGain(float gainDB) {
    float linear = dbToLinear(gainDB);
    m_inputGainLinear.store(linear, std::memory_order_relaxed);
    m_inputGainLinearL.store(linear, std::memory_order_relaxed);
    m_inputGainLinearR.store(linear, std::memory_order_relaxed);
}

void Engine::setInputGainL(float gainDB) {
    m_inputGainLinearL.store(dbToLinear(gainDB), std::memory_order_relaxed);
}

void Engine::setInputGainR(float gainDB) {
    m_inputGainLinearR.store(dbToLinear(gainDB), std::memory_order_relaxed);
}

void Engine::setOutputGain(float gainDB) {
    float linear = dbToLinear(gainDB);
    m_outputGainLinear.store(linear, std::memory_order_relaxed);
    m_outputGainLinearL.store(linear, std::memory_order_relaxed);
    m_outputGainLinearR.store(linear, std::memory_order_relaxed);
}

void Engine::setOutputGainL(float gainDB) {
    m_outputGainLinearL.store(dbToLinear(gainDB), std::memory_order_relaxed);
}

void Engine::setOutputGainR(float gainDB) {
    m_outputGainLinearR.store(dbToLinear(gainDB), std::memory_order_relaxed);
}

void Engine::setBypass(bool bypass) {
    m_bypass.store(bypass, std::memory_order_relaxed);
}

void Engine::setBypassModel(bool bypass) {
    m_bypassModel.store(bypass, std::memory_order_relaxed);
}

void Engine::setNormalize(bool normalize) {
    m_normalize.store(normalize, std::memory_order_relaxed);
}

void Engine::setNoiseGate(bool enabled, float thresholdDB) {
    m_noiseGateEnabled.store(enabled, std::memory_order_relaxed);
    m_noiseGateEnabledL.store(enabled, std::memory_order_relaxed);
    m_noiseGateEnabledR.store(enabled, std::memory_order_relaxed);
    m_noiseGate->setThreshold(thresholdDB);
    m_noiseGateL->setThreshold(thresholdDB);
    m_noiseGateR->setThreshold(thresholdDB);
}

void Engine::setNoiseGateL(bool enabled, float thresholdDB) {
    m_noiseGateEnabledL.store(enabled, std::memory_order_relaxed);
    m_noiseGateL->setThreshold(thresholdDB);
}

void Engine::setNoiseGateR(bool enabled, float thresholdDB) {
    m_noiseGateEnabledR.store(enabled, std::memory_order_relaxed);
    m_noiseGateR->setThreshold(thresholdDB);
}

void Engine::setDCBlocker(bool enabled) {
    m_dcBlockerEnabled.store(enabled, std::memory_order_relaxed);
}

void Engine::setEQEnabled(bool enabled) {
    m_eq->setEnabled(enabled);
    m_eqL->setEnabled(enabled);
    m_eqR->setEnabled(enabled);
}

void Engine::setEQBass(float gainDB) {
    m_eq->setBass(gainDB);
    m_eqL->setBass(gainDB);
    m_eqR->setBass(gainDB);
}

void Engine::setEQMid(float gainDB) {
    m_eq->setMid(gainDB);
    m_eqL->setMid(gainDB);
    m_eqR->setMid(gainDB);
}

void Engine::setEQTreble(float gainDB) {
    m_eq->setTreble(gainDB);
    m_eqL->setTreble(gainDB);
    m_eqR->setTreble(gainDB);
}

void Engine::setEQEnabledL(bool enabled) {
    m_eqL->setEnabled(enabled);
}

void Engine::setEQBassL(float gainDB) {
    m_eqL->setBass(gainDB);
}

void Engine::setEQMidL(float gainDB) {
    m_eqL->setMid(gainDB);
}

void Engine::setEQTrebleL(float gainDB) {
    m_eqL->setTreble(gainDB);
}

void Engine::setEQEnabledR(bool enabled) {
    m_eqR->setEnabled(enabled);
}

void Engine::setEQBassR(float gainDB) {
    m_eqR->setBass(gainDB);
}

void Engine::setEQMidR(float gainDB) {
    m_eqR->setMid(gainDB);
}

void Engine::setEQTrebleR(float gainDB) {
    m_eqR->setTreble(gainDB);
}

// ===== Getters =====

float Engine::getInputGain() const {
    return linearToDb(m_inputGainLinear.load(std::memory_order_relaxed));
}

float Engine::getInputGainL() const {
    return linearToDb(m_inputGainLinearL.load(std::memory_order_relaxed));
}

float Engine::getInputGainR() const {
    return linearToDb(m_inputGainLinearR.load(std::memory_order_relaxed));
}

float Engine::getOutputGain() const {
    return linearToDb(m_outputGainLinear.load(std::memory_order_relaxed));
}

float Engine::getOutputGainL() const {
    return linearToDb(m_outputGainLinearL.load(std::memory_order_relaxed));
}

float Engine::getOutputGainR() const {
    return linearToDb(m_outputGainLinearR.load(std::memory_order_relaxed));
}

bool Engine::getBypass() const {
    return m_bypass.load(std::memory_order_relaxed);
}

bool Engine::getBypassModel() const {
    return m_bypassModel.load(std::memory_order_relaxed);
}

bool Engine::getNormalize() const {
    return m_normalize.load(std::memory_order_relaxed);
}

bool Engine::getNoiseGateEnabled() const {
    return m_noiseGateEnabled.load(std::memory_order_relaxed);
}

bool Engine::getNoiseGateEnabledL() const {
    return m_noiseGateEnabledL.load(std::memory_order_relaxed);
}

bool Engine::getNoiseGateEnabledR() const {
    return m_noiseGateEnabledR.load(std::memory_order_relaxed);
}

float Engine::getNoiseGateThreshold() const {
    return m_noiseGate->getThreshold();
}

float Engine::getNoiseGateThresholdL() const {
    return m_noiseGateL->getThreshold();
}

float Engine::getNoiseGateThresholdR() const {
    return m_noiseGateR->getThreshold();
}

bool Engine::getDCBlockerEnabled() const {
    return m_dcBlockerEnabled.load(std::memory_order_relaxed);
}

bool Engine::getEQEnabled() const {
    return m_eq->getEnabled();
}

bool Engine::getEQEnabledL() const {
    return m_eqL->getEnabled();
}

bool Engine::getEQEnabledR() const {
    return m_eqR->getEnabled();
}

float Engine::getEQBass() const {
    return m_eq->getBass();
}

float Engine::getEQBassL() const {
    return m_eqL->getBass();
}

float Engine::getEQBassR() const {
    return m_eqR->getBass();
}

float Engine::getEQMid() const {
    return m_eq->getMid();
}

float Engine::getEQMidL() const {
    return m_eqL->getMid();
}

float Engine::getEQMidR() const {
    return m_eqR->getMid();
}

float Engine::getEQTreble() const {
    return m_eq->getTreble();
}

float Engine::getEQTrebleL() const {
    return m_eqL->getTreble();
}

float Engine::getEQTrebleR() const {
    return m_eqR->getTreble();
}

// ===== Monitoring =====

uint32_t Engine::getXrunCount() const {
    return m_xrunCount.load(std::memory_order_relaxed);
}

void Engine::resetXrunCount() {
    m_xrunCount.store(0, std::memory_order_relaxed);
}

float Engine::getLatency() const {
    // Base latency is buffer size
    // TODO: Add model latency if available
    return static_cast<float>(m_bufferSize);
}

// ===== Callbacks =====

void Engine::setModelLoadCallback(ModelLoadCallback callback) {
    m_modelLoadCallback = std::move(callback);
}

// ===== Private Helpers =====

void Engine::smoothGains() {
    // Smooth gain changes to prevent clicks - legacy
    float targetInput = m_inputGainLinear.load(std::memory_order_relaxed);
    float targetOutput = m_outputGainLinear.load(std::memory_order_relaxed);

    m_currentInputGain = m_currentInputGain * GAIN_SMOOTH_COEFF +
                         targetInput * (1.0f - GAIN_SMOOTH_COEFF);

    m_currentOutputGain = m_currentOutputGain * GAIN_SMOOTH_COEFF +
                          targetOutput * (1.0f - GAIN_SMOOTH_COEFF);

    // Smooth gain changes to prevent clicks - per channel
    float targetInputL = m_inputGainLinearL.load(std::memory_order_relaxed);
    float targetOutputL = m_outputGainLinearL.load(std::memory_order_relaxed);
    float targetInputR = m_inputGainLinearR.load(std::memory_order_relaxed);
    float targetOutputR = m_outputGainLinearR.load(std::memory_order_relaxed);

    m_currentInputGainL = m_currentInputGainL * GAIN_SMOOTH_COEFF +
                          targetInputL * (1.0f - GAIN_SMOOTH_COEFF);

    m_currentOutputGainL = m_currentOutputGainL * GAIN_SMOOTH_COEFF +
                           targetOutputL * (1.0f - GAIN_SMOOTH_COEFF);

    m_currentInputGainR = m_currentInputGainR * GAIN_SMOOTH_COEFF +
                          targetInputR * (1.0f - GAIN_SMOOTH_COEFF);

    m_currentOutputGainR = m_currentOutputGainR * GAIN_SMOOTH_COEFF +
                           targetOutputR * (1.0f - GAIN_SMOOTH_COEFF);
}

float Engine::dbToLinear(float db) const {
    return std::pow(10.0f, db / 20.0f);
}

float Engine::linearToDb(float linear) const {
    if (linear <= 0.0f) return -100.0f; // -inf
    return 20.0f * std::log10(linear);
}

// ===== Recording =====

std::string Engine::startRecording(const std::string& filename) {
    if (!m_recorder) {
        return "";
    }
    return m_recorder->startRecording(filename, m_sampleRate);
}

void Engine::stopRecording() {
    if (m_recorder) {
        m_recorder->stopRecording();
    }
}

bool Engine::isRecording() const {
    return m_recorder && m_recorder->isRecording();
}

std::string Engine::getRecordingFilePath() const {
    if (!m_recorder) {
        return "";
    }
    return m_recorder->getCurrentFilePath();
}

uint64_t Engine::getRecordingDroppedFrames() const {
    if (!m_recorder) {
        return 0;
    }
    return m_recorder->getDroppedFrames();
}

double Engine::getRecordingDuration() const {
    if (!m_recorder) {
        return 0.0;
    }
    return m_recorder->getRecordingDuration();
}

// ===== Backing Track Recording =====

void Engine::setBackingTrack(class BackingTrack* backingTrack) {
    m_backingTrack = backingTrack;
}

void Engine::setIncludeBackingTrackInRecording(bool enabled) {
    m_includeBackingTrackInRecording.store(enabled);
}

bool Engine::getIncludeBackingTrackInRecording() const {
    return m_includeBackingTrackInRecording.load();
}

// ===== Reverb =====

void Engine::setReverbEnabled(bool enabled) {
    if (m_reverb) {
        m_reverb->setEnabled(enabled);
    }
}

void Engine::setReverbRoomSize(float size) {
    if (m_reverb) {
        m_reverb->setRoomSize(size);
    }
}

void Engine::setReverbDecayTime(float seconds) {
    if (m_reverb) {
        m_reverb->setDecayTime(seconds);
    }
}

void Engine::setReverbMix(float dry, float wet) {
    if (m_reverb) {
        m_reverb->setMix(dry, wet);
    }
}

void Engine::clearReverbBuffers() {
    if (m_reverb) {
        m_reverb->clearBuffers();
    }
}

bool Engine::getReverbEnabled() const {
    return m_reverb ? m_reverb->getEnabled() : false;
}

float Engine::getReverbRoomSize() const {
    return m_reverb ? m_reverb->getRoomSize() : 0.3f;
}

float Engine::getReverbDecayTime() const {
    return m_reverb ? m_reverb->getDecayTime() : 2.0f;
}

float Engine::getReverbDry() const {
    return m_reverb ? m_reverb->getDry() : 1.0f;
}

float Engine::getReverbWet() const {
    return m_reverb ? m_reverb->getWet() : 0.3f;
}

} // namespace HoopiPi
