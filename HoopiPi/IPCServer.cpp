#include "IPCServer.h"
#include "JackBackend.h"
#include "ConfigPersistence.h"
#include "../standalone/BackingTrack.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <filesystem>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace HoopiPi {

IPCServer::IPCServer(Engine* engine)
    : m_engine(engine)
    , m_backend(nullptr)
    , m_backingTrack(nullptr)
    , m_serverSocket(-1)
    , m_running(false)
{
}

void IPCServer::setBackend(JackBackend* backend) {
    m_backend = backend;
}

void IPCServer::setBackingTrack(BackingTrack* backingTrack) {
    m_backingTrack = backingTrack;
}

void IPCServer::setStatusChangeCallback(StatusChangeCallback callback) {
    m_statusChangeCallback = callback;
}

void IPCServer::notifyStatusChange() {
    if (m_statusChangeCallback) {
        m_statusChangeCallback();
    }
}

IPCServer::~IPCServer() {
    stop();
}

bool IPCServer::start(const std::string& socketPath) {
    if (m_running.load()) {
        std::cerr << "IPC server already running" << std::endl;
        return false;
    }

    m_socketPath = socketPath;

    // Remove existing socket file
    unlink(m_socketPath.c_str());

    // Create socket
    m_serverSocket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_serverSocket < 0) {
        std::cerr << "Failed to create IPC socket" << std::endl;
        return false;
    }

    // Bind socket
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, m_socketPath.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(m_serverSocket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Failed to bind IPC socket to " << m_socketPath << std::endl;
        close(m_serverSocket);
        m_serverSocket = -1;
        return false;
    }

    // Listen
    if (listen(m_serverSocket, 5) < 0) {
        std::cerr << "Failed to listen on IPC socket" << std::endl;
        close(m_serverSocket);
        m_serverSocket = -1;
        unlink(m_socketPath.c_str());
        return false;
    }

    m_running.store(true);
    m_serverThread = std::make_unique<std::thread>(&IPCServer::serverLoop, this);

    std::cout << "IPC server started on " << m_socketPath << std::endl;
    return true;
}

void IPCServer::stop() {
    if (!m_running.load()) {
        return;
    }

    m_running.store(false);

    // Close server socket to unblock accept()
    if (m_serverSocket >= 0) {
        shutdown(m_serverSocket, SHUT_RDWR);
        close(m_serverSocket);
        m_serverSocket = -1;
    }

    // Wait for thread to finish
    if (m_serverThread && m_serverThread->joinable()) {
        m_serverThread->join();
    }

    // Remove socket file
    unlink(m_socketPath.c_str());

    std::cout << "IPC server stopped" << std::endl;
}

void IPCServer::serverLoop() {
    while (m_running.load()) {
        struct sockaddr_un clientAddr;
        socklen_t clientLen = sizeof(clientAddr);

        int clientSocket = accept(m_serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientSocket < 0) {
            if (m_running.load()) {
                std::cerr << "IPC accept error" << std::endl;
            }
            break;
        }

        // Handle client request in same thread (simple for now)
        handleClient(clientSocket);
        close(clientSocket);
    }
}

void IPCServer::handleClient(int clientSocket) {
    char buffer[4096];
    ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

    if (bytesRead <= 0) {
        return;
    }

    buffer[bytesRead] = '\0';
    std::string command(buffer);

    std::string response = handleCommand(command);

    send(clientSocket, response.c_str(), response.length(), 0);
}

std::string IPCServer::handleCommand(const std::string& command) {
    json response;

    try {
        json cmd = json::parse(command);

        std::string action = cmd.value("action", "");

        if (action == "loadModel") {
            std::string modelPath = cmd.value("modelPath", "");
            int slot = cmd.value("slot", 0);

            if (modelPath.empty()) {
                response["success"] = false;
                response["error"] = "Missing modelPath";
            } else if (slot < 0 || slot > 1) {
                response["success"] = false;
                response["error"] = "Invalid slot (must be 0 or 1)";
            } else {
                m_engine->loadModelAsync(slot, modelPath);

                // Save model configuration for persistence
                ConfigPersistence::saveModelConfig(modelPath, slot);

                // Disable bypass when model is loaded
                m_engine->setBypass(false);

                response["success"] = true;
                response["message"] = "Model loading started";
                response["slot"] = slot;
                response["modelPath"] = modelPath;
            }
        }
        else if (action == "setActiveModel") {
            int slot = cmd.value("slot", 0);

            if (slot < 0 || slot > 1) {
                response["success"] = false;
                response["error"] = "Invalid slot (must be 0 or 1)";
            } else {
                m_engine->setActiveModel(slot);

                // Save active slot for persistence
                ConfigPersistence::saveActiveSlot(slot);

                response["success"] = true;
                response["message"] = "Active model set";
                response["slot"] = slot;
                notifyStatusChange();
            }
        }
        else if (action == "setActiveModelL") {
            int slot = cmd.value("slot", 0);

            if (slot < 0 || slot > 1) {
                response["success"] = false;
                response["error"] = "Invalid slot (must be 0 or 1)";
            } else {
                m_engine->setActiveModelL(slot);
                response["success"] = true;
                response["message"] = "Left channel active model set";
                response["slot"] = slot;
                notifyStatusChange();
            }
        }
        else if (action == "setActiveModelR") {
            int slot = cmd.value("slot", 0);

            if (slot < 0 || slot > 1) {
                response["success"] = false;
                response["error"] = "Invalid slot (must be 0 or 1)";
            } else {
                m_engine->setActiveModelR(slot);
                response["success"] = true;
                response["message"] = "Right channel active model set";
                response["slot"] = slot;
                notifyStatusChange();
            }
        }
        else if (action == "setBypassModelL") {
            bool bypass = cmd.value("bypass", false);
            m_engine->setBypassModelL(bypass);

            // Save bypass states for persistence
            ConfigPersistence::saveBypassStates(bypass, m_engine->getBypassModelR());

            response["success"] = true;
            response["message"] = "Left channel model bypass set";
            response["bypassModelL"] = bypass;
            notifyStatusChange();
        }
        else if (action == "setBypassModelR") {
            bool bypass = cmd.value("bypass", false);
            m_engine->setBypassModelR(bypass);

            // Save bypass states for persistence
            ConfigPersistence::saveBypassStates(m_engine->getBypassModelL(), bypass);

            response["success"] = true;
            response["message"] = "Right channel model bypass set";
            response["bypassModelR"] = bypass;
            notifyStatusChange();
        }
        else if (action == "unloadModel") {
            int slot = cmd.value("slot", 0);

            if (slot < 0 || slot > 1) {
                response["success"] = false;
                response["error"] = "Invalid slot (must be 0 or 1)";
            } else {
                m_engine->unloadModel(slot);

                // Clear saved model configuration for this slot
                ConfigPersistence::clearSlotConfig(slot);

                // Enable bypass when model is unloaded
                m_engine->setBypass(true);

                response["success"] = true;
                response["message"] = "Model unloaded";
                response["slot"] = slot;
                notifyStatusChange();
            }
        }
        else if (action == "getStatus") {
            response["success"] = true;
            response["activeModel"] = m_engine->getActiveModel();
            response["activeModelL"] = m_engine->getActiveModelL();
            response["activeModelR"] = m_engine->getActiveModelR();
            response["modelReady"] = {
                m_engine->isModelReady(0),
                m_engine->isModelReady(1)
            };

            // Get model names (filenames only, not full paths)
            json modelNames = json::array();
            for (int i = 0; i < 2; i++) {
                std::string path = m_engine->getModelPath(i);
                if (!path.empty()) {
                    std::filesystem::path p(path);
                    modelNames.push_back(p.filename().string());
                } else {
                    modelNames.push_back("");
                }
            }
            response["modelNames"] = modelNames;

            // Stereo mode
            StereoMode stereoMode = m_engine->getStereoMode();
            std::string stereoModeStr;
            switch (stereoMode) {
                case StereoMode::LeftMono2Stereo:
                    stereoModeStr = "LeftMono2Stereo";
                    break;
                case StereoMode::Stereo2Stereo:
                    stereoModeStr = "Stereo2Stereo";
                    break;
                case StereoMode::RightMono2Stereo:
                    stereoModeStr = "RightMono2Stereo";
                    break;
                case StereoMode::Stereo2Mono:
                    stereoModeStr = "Stereo2Mono";
                    break;
            }
            response["stereoMode"] = stereoModeStr;
            response["stereo2MonoMixL"] = m_engine->getStereo2MonoMixL();
            response["stereo2MonoMixR"] = m_engine->getStereo2MonoMixR();

            // Legacy parameters (for backwards compatibility)
            response["inputGain"] = m_engine->getInputGain();
            response["outputGain"] = m_engine->getOutputGain();
            response["bypass"] = m_engine->getBypass();
            response["bypassModel"] = m_engine->getBypassModel();
            response["bypassModelL"] = m_engine->getBypassModelL();
            response["bypassModelR"] = m_engine->getBypassModelR();
            response["normalize"] = m_engine->getNormalize();
            response["noiseGateEnabled"] = m_engine->getNoiseGateEnabled();
            response["noiseGateThreshold"] = m_engine->getNoiseGateThreshold();
            response["dcBlockerEnabled"] = m_engine->getDCBlockerEnabled();
            response["eqEnabled"] = m_engine->getEQEnabled();
            response["eqBass"] = m_engine->getEQBass();
            response["eqMid"] = m_engine->getEQMid();
            response["eqTreble"] = m_engine->getEQTreble();

            // Per-channel parameters
            response["inputGainL"] = m_engine->getInputGainL();
            response["inputGainR"] = m_engine->getInputGainR();
            response["outputGainL"] = m_engine->getOutputGainL();
            response["outputGainR"] = m_engine->getOutputGainR();
            response["noiseGateEnabledL"] = m_engine->getNoiseGateEnabledL();
            response["noiseGateThresholdL"] = m_engine->getNoiseGateThresholdL();
            response["noiseGateEnabledR"] = m_engine->getNoiseGateEnabledR();
            response["noiseGateThresholdR"] = m_engine->getNoiseGateThresholdR();
            response["eqEnabledL"] = m_engine->getEQEnabledL();
            response["eqBassL"] = m_engine->getEQBassL();
            response["eqMidL"] = m_engine->getEQMidL();
            response["eqTrebleL"] = m_engine->getEQTrebleL();
            response["eqEnabledR"] = m_engine->getEQEnabledR();
            response["eqBassR"] = m_engine->getEQBassR();
            response["eqMidR"] = m_engine->getEQMidR();
            response["eqTrebleR"] = m_engine->getEQTrebleR();

            // Get recording status
            response["recording"] = m_engine->isRecording();
            if (m_engine->isRecording()) {
                response["recordingFile"] = m_engine->getRecordingFilePath();
                response["recordingDroppedFrames"] = m_engine->getRecordingDroppedFrames();
                response["recordingDuration"] = m_engine->getRecordingDuration();
            }

            // Get reverb status
            response["reverbEnabled"] = m_engine->getReverbEnabled();
            response["reverbRoomSize"] = m_engine->getReverbRoomSize();
            response["reverbDecayTime"] = m_engine->getReverbDecayTime();
            response["reverbDry"] = m_engine->getReverbDry();
            response["reverbWet"] = m_engine->getReverbWet();

            // Get CPU load, temperature, memory, and xruns from backend if available
            if (m_backend) {
                // JACK connection status
                JackStatus jackStatus = m_backend->getStatus();
                std::string statusStr;
                switch (jackStatus) {
                    case JackStatus::Connected:
                        statusStr = "connected";
                        break;
                    case JackStatus::Connecting:
                        statusStr = "connecting";
                        break;
                    case JackStatus::Disconnected:
                        statusStr = "disconnected";
                        break;
                    case JackStatus::Error:
                        statusStr = "error";
                        break;
                }
                response["jackStatus"] = statusStr;
                response["jackError"] = m_backend->getErrorMessage();

                // Only include audio metrics if connected
                if (jackStatus == JackStatus::Connected) {
                    response["cpuLoad"] = m_backend->getCPULoad();  // JACK DSP load
                    response["processCpu"] = m_backend->getProcessCPUUsage();  // HoopiPi CPU usage
                    response["sampleRate"] = m_backend->getSampleRate();
                    response["bufferSize"] = m_backend->getBufferSize();
                    response["latencyMs"] = m_backend->getLatencyMs();
                } else {
                    response["cpuLoad"] = 0.0f;
                    response["processCpu"] = 0.0f;
                    response["sampleRate"] = 0;
                    response["bufferSize"] = 0;
                    response["latencyMs"] = 0.0f;
                }

                response["cpuTemp"] = m_backend->getCPUTemperature();
                response["memoryUsage"] = m_backend->getMemoryUsage();
                response["xruns"] = m_backend->getXrunCount();
            } else {
                response["jackStatus"] = "unknown";
                response["jackError"] = "";
                response["cpuLoad"] = 0.0f;
                response["cpuTemp"] = -1.0f;
                response["memoryUsage"] = -1.0f;
                response["xruns"] = 0;
                response["sampleRate"] = 0;
                response["bufferSize"] = 0;
                response["latencyMs"] = 0.0f;
            }
        }
        else if (action == "setInputGain") {
            float gain = cmd.value("gain", 0.0f);
            m_engine->setInputGain(gain);

            // Save gain settings for persistence
            ConfigPersistence::saveGainSettings(gain, m_engine->getOutputGain());

            response["success"] = true;
            response["message"] = "Input gain set";
            response["gain"] = gain;
            notifyStatusChange();
        }
        else if (action == "setOutputGain") {
            float gain = cmd.value("gain", 0.0f);
            m_engine->setOutputGain(gain);

            // Save gain settings for persistence
            ConfigPersistence::saveGainSettings(m_engine->getInputGain(), gain);

            response["success"] = true;
            response["message"] = "Output gain set";
            response["gain"] = gain;
            notifyStatusChange();
        }
        else if (action == "setBypass") {
            bool bypass = cmd.value("bypass", false);
            m_engine->setBypass(bypass);
            response["success"] = true;
            response["message"] = "Bypass set";
            response["bypass"] = bypass;
            notifyStatusChange();
        }
        else if (action == "setBypassModel") {
            bool bypass = cmd.value("bypass", false);
            m_engine->setBypassModel(bypass);
            response["success"] = true;
            response["message"] = "Model bypass set";
            response["bypassModel"] = bypass;
            notifyStatusChange();
        }
        else if (action == "setEQEnabled") {
            bool enabled = cmd.value("enabled", false);
            m_engine->setEQEnabled(enabled);

            // Save EQ settings for persistence
            ConfigPersistence::saveEQSettings(enabled, m_engine->getEQBass(),
                m_engine->getEQMid(), m_engine->getEQTreble());

            response["success"] = true;
            response["message"] = "EQ enabled set";
            response["enabled"] = enabled;
            notifyStatusChange();
        }
        else if (action == "setEQBass") {
            float gain = cmd.value("gain", 0.0f);
            m_engine->setEQBass(gain);

            // Save EQ settings for persistence
            ConfigPersistence::saveEQSettings(m_engine->getEQEnabled(), gain,
                m_engine->getEQMid(), m_engine->getEQTreble());

            response["success"] = true;
            response["message"] = "EQ bass set";
            response["gain"] = gain;
            notifyStatusChange();
        }
        else if (action == "setEQMid") {
            float gain = cmd.value("gain", 0.0f);
            m_engine->setEQMid(gain);

            // Save EQ settings for persistence
            ConfigPersistence::saveEQSettings(m_engine->getEQEnabled(),
                m_engine->getEQBass(), gain, m_engine->getEQTreble());

            response["success"] = true;
            response["message"] = "EQ mid set";
            response["gain"] = gain;
            notifyStatusChange();
        }
        else if (action == "setEQTreble") {
            float gain = cmd.value("gain", 0.0f);
            m_engine->setEQTreble(gain);

            // Save EQ settings for persistence
            ConfigPersistence::saveEQSettings(m_engine->getEQEnabled(),
                m_engine->getEQBass(), m_engine->getEQMid(), gain);

            response["success"] = true;
            response["message"] = "EQ treble set";
            response["gain"] = gain;
            notifyStatusChange();
        }
        else if (action == "setNoiseGateEnabled") {
            bool enabled = cmd.value("enabled", false);
            float threshold = m_engine->getNoiseGateThreshold();
            m_engine->setNoiseGate(enabled, threshold);

            // Save noise gate settings for persistence
            ConfigPersistence::saveNoiseGateSettings(enabled, threshold);

            response["success"] = true;
            response["message"] = "Noise gate enabled set";
            response["enabled"] = enabled;
            notifyStatusChange();
        }
        else if (action == "setNoiseGateThreshold") {
            float threshold = cmd.value("threshold", -40.0f);
            bool enabled = m_engine->getNoiseGateEnabled();
            m_engine->setNoiseGate(enabled, threshold);

            // Save noise gate settings for persistence
            ConfigPersistence::saveNoiseGateSettings(enabled, threshold);

            response["success"] = true;
            response["message"] = "Noise gate threshold set";
            response["threshold"] = threshold;
            notifyStatusChange();
        }
        else if (action == "setStereoMode") {
            std::string modeStr = cmd.value("mode", "LeftMono2Stereo");
            StereoMode mode = StereoMode::LeftMono2Stereo;
            if (modeStr == "Stereo2Stereo") {
                mode = StereoMode::Stereo2Stereo;
            } else if (modeStr == "RightMono2Stereo") {
                mode = StereoMode::RightMono2Stereo;
            } else if (modeStr == "Stereo2Mono") {
                mode = StereoMode::Stereo2Mono;
            }
            m_engine->setStereoMode(mode);

            // Save stereo mode for persistence
            ConfigPersistence::saveStereoMode(modeStr);

            response["success"] = true;
            response["message"] = "Stereo mode set";
            response["mode"] = modeStr;
            notifyStatusChange();
        }
        else if (action == "setStereo2MonoMixL") {
            float level = cmd.value("level", 0.5f);
            m_engine->setStereo2MonoMixL(level);

            // Save stereo2mono mix levels for persistence
            ConfigPersistence::saveStereo2MonoMix(level, m_engine->getStereo2MonoMixR());

            response["success"] = true;
            response["message"] = "Stereo2Mono mix L set";
            response["level"] = level;
            notifyStatusChange();
        }
        else if (action == "setStereo2MonoMixR") {
            float level = cmd.value("level", 0.5f);
            m_engine->setStereo2MonoMixR(level);

            // Save stereo2mono mix levels for persistence
            ConfigPersistence::saveStereo2MonoMix(m_engine->getStereo2MonoMixL(), level);

            response["success"] = true;
            response["message"] = "Stereo2Mono mix R set";
            response["level"] = level;
            notifyStatusChange();
        }
        else if (action == "setInputGainL") {
            float gain = cmd.value("gain", 0.0f);
            m_engine->setInputGainL(gain);

            // Save per-channel gain settings for persistence
            ConfigPersistence::savePerChannelGains(gain, m_engine->getInputGainR(),
                m_engine->getOutputGainL(), m_engine->getOutputGainR());

            response["success"] = true;
            response["message"] = "Left input gain set";
            response["gain"] = gain;
            notifyStatusChange();
        }
        else if (action == "setInputGainR") {
            float gain = cmd.value("gain", 0.0f);
            m_engine->setInputGainR(gain);

            // Save per-channel gain settings for persistence
            ConfigPersistence::savePerChannelGains(m_engine->getInputGainL(), gain,
                m_engine->getOutputGainL(), m_engine->getOutputGainR());

            response["success"] = true;
            response["message"] = "Right input gain set";
            response["gain"] = gain;
            notifyStatusChange();
        }
        else if (action == "setOutputGainL") {
            float gain = cmd.value("gain", 0.0f);
            m_engine->setOutputGainL(gain);

            // Save per-channel gain settings for persistence
            ConfigPersistence::savePerChannelGains(m_engine->getInputGainL(), m_engine->getInputGainR(),
                gain, m_engine->getOutputGainR());

            response["success"] = true;
            response["message"] = "Left output gain set";
            response["gain"] = gain;
            notifyStatusChange();
        }
        else if (action == "setOutputGainR") {
            float gain = cmd.value("gain", 0.0f);
            m_engine->setOutputGainR(gain);

            // Save per-channel gain settings for persistence
            ConfigPersistence::savePerChannelGains(m_engine->getInputGainL(), m_engine->getInputGainR(),
                m_engine->getOutputGainL(), gain);

            response["success"] = true;
            response["message"] = "Right output gain set";
            response["gain"] = gain;
            notifyStatusChange();
        }
        else if (action == "setNoiseGateL") {
            bool enabled = cmd.value("enabled", false);
            float threshold = cmd.value("threshold", -40.0f);
            m_engine->setNoiseGateL(enabled, threshold);

            // Save per-channel noise gate settings for persistence
            ConfigPersistence::savePerChannelNoiseGate(enabled, threshold,
                m_engine->getNoiseGateEnabledR(), m_engine->getNoiseGateThresholdR());

            response["success"] = true;
            response["message"] = "Left noise gate set";
            response["enabled"] = enabled;
            response["threshold"] = threshold;
            notifyStatusChange();
        }
        else if (action == "setNoiseGateR") {
            bool enabled = cmd.value("enabled", false);
            float threshold = cmd.value("threshold", -40.0f);
            m_engine->setNoiseGateR(enabled, threshold);

            // Save per-channel noise gate settings for persistence
            ConfigPersistence::savePerChannelNoiseGate(m_engine->getNoiseGateEnabledL(), m_engine->getNoiseGateThresholdL(),
                enabled, threshold);

            response["success"] = true;
            response["message"] = "Right noise gate set";
            response["enabled"] = enabled;
            response["threshold"] = threshold;
            notifyStatusChange();
        }
        else if (action == "setEQEnabledL") {
            bool enabled = cmd.value("enabled", false);
            m_engine->setEQEnabledL(enabled);

            // Save per-channel EQ settings for persistence
            ConfigPersistence::savePerChannelEQ(enabled, m_engine->getEQBassL(), m_engine->getEQMidL(), m_engine->getEQTrebleL(),
                m_engine->getEQEnabledR(), m_engine->getEQBassR(), m_engine->getEQMidR(), m_engine->getEQTrebleR());

            response["success"] = true;
            response["message"] = "Left EQ enabled set";
            response["enabled"] = enabled;
            notifyStatusChange();
        }
        else if (action == "setEQBassL") {
            float gain = cmd.value("gain", 0.0f);
            m_engine->setEQBassL(gain);

            // Save per-channel EQ settings for persistence
            ConfigPersistence::savePerChannelEQ(m_engine->getEQEnabledL(), gain, m_engine->getEQMidL(), m_engine->getEQTrebleL(),
                m_engine->getEQEnabledR(), m_engine->getEQBassR(), m_engine->getEQMidR(), m_engine->getEQTrebleR());

            response["success"] = true;
            response["message"] = "Left EQ bass set";
            response["gain"] = gain;
            notifyStatusChange();
        }
        else if (action == "setEQMidL") {
            float gain = cmd.value("gain", 0.0f);
            m_engine->setEQMidL(gain);

            // Save per-channel EQ settings for persistence
            ConfigPersistence::savePerChannelEQ(m_engine->getEQEnabledL(), m_engine->getEQBassL(), gain, m_engine->getEQTrebleL(),
                m_engine->getEQEnabledR(), m_engine->getEQBassR(), m_engine->getEQMidR(), m_engine->getEQTrebleR());

            response["success"] = true;
            response["message"] = "Left EQ mid set";
            response["gain"] = gain;
            notifyStatusChange();
        }
        else if (action == "setEQTrebleL") {
            float gain = cmd.value("gain", 0.0f);
            m_engine->setEQTrebleL(gain);

            // Save per-channel EQ settings for persistence
            ConfigPersistence::savePerChannelEQ(m_engine->getEQEnabledL(), m_engine->getEQBassL(), m_engine->getEQMidL(), gain,
                m_engine->getEQEnabledR(), m_engine->getEQBassR(), m_engine->getEQMidR(), m_engine->getEQTrebleR());

            response["success"] = true;
            response["message"] = "Left EQ treble set";
            response["gain"] = gain;
            notifyStatusChange();
        }
        else if (action == "setEQEnabledR") {
            bool enabled = cmd.value("enabled", false);
            m_engine->setEQEnabledR(enabled);

            // Save per-channel EQ settings for persistence
            ConfigPersistence::savePerChannelEQ(m_engine->getEQEnabledL(), m_engine->getEQBassL(), m_engine->getEQMidL(), m_engine->getEQTrebleL(),
                enabled, m_engine->getEQBassR(), m_engine->getEQMidR(), m_engine->getEQTrebleR());

            response["success"] = true;
            response["message"] = "Right EQ enabled set";
            response["enabled"] = enabled;
            notifyStatusChange();
        }
        else if (action == "setEQBassR") {
            float gain = cmd.value("gain", 0.0f);
            m_engine->setEQBassR(gain);

            // Save per-channel EQ settings for persistence
            ConfigPersistence::savePerChannelEQ(m_engine->getEQEnabledL(), m_engine->getEQBassL(), m_engine->getEQMidL(), m_engine->getEQTrebleL(),
                m_engine->getEQEnabledR(), gain, m_engine->getEQMidR(), m_engine->getEQTrebleR());

            response["success"] = true;
            response["message"] = "Right EQ bass set";
            response["gain"] = gain;
            notifyStatusChange();
        }
        else if (action == "setEQMidR") {
            float gain = cmd.value("gain", 0.0f);
            m_engine->setEQMidR(gain);

            // Save per-channel EQ settings for persistence
            ConfigPersistence::savePerChannelEQ(m_engine->getEQEnabledL(), m_engine->getEQBassL(), m_engine->getEQMidL(), m_engine->getEQTrebleL(),
                m_engine->getEQEnabledR(), m_engine->getEQBassR(), gain, m_engine->getEQTrebleR());

            response["success"] = true;
            response["message"] = "Right EQ mid set";
            response["gain"] = gain;
            notifyStatusChange();
        }
        else if (action == "setEQTrebleR") {
            float gain = cmd.value("gain", 0.0f);
            m_engine->setEQTrebleR(gain);

            // Save per-channel EQ settings for persistence
            ConfigPersistence::savePerChannelEQ(m_engine->getEQEnabledL(), m_engine->getEQBassL(), m_engine->getEQMidL(), m_engine->getEQTrebleL(),
                m_engine->getEQEnabledR(), m_engine->getEQBassR(), m_engine->getEQMidR(), gain);

            response["success"] = true;
            response["message"] = "Right EQ treble set";
            response["gain"] = gain;
            notifyStatusChange();
        }
        else if (action == "startRecording") {
            std::string filename = cmd.value("filename", "");
            std::string filepath = m_engine->startRecording(filename);

            if (!filepath.empty()) {
                response["success"] = true;
                response["message"] = "Recording started";
                response["filepath"] = filepath;
            } else {
                response["success"] = false;
                response["error"] = "Failed to start recording";
            }
        }
        else if (action == "stopRecording") {
            m_engine->stopRecording();
            response["success"] = true;
            response["message"] = "Recording stopped";
        }
        else if (action == "setReverbEnabled") {
            bool enabled = cmd.value("enabled", false);
            m_engine->setReverbEnabled(enabled);

            // Save reverb settings for persistence
            ConfigPersistence::saveReverbSettings(enabled, m_engine->getReverbRoomSize(),
                m_engine->getReverbDecayTime(), m_engine->getReverbDry(), m_engine->getReverbWet());

            response["success"] = true;
            response["message"] = "Reverb enabled set";
            response["enabled"] = enabled;
            notifyStatusChange();
        }
        else if (action == "setReverbRoomSize") {
            float size = cmd.value("size", 0.3f);
            m_engine->setReverbRoomSize(size);

            // Save reverb settings for persistence
            ConfigPersistence::saveReverbSettings(m_engine->getReverbEnabled(), size,
                m_engine->getReverbDecayTime(), m_engine->getReverbDry(), m_engine->getReverbWet());

            response["success"] = true;
            response["message"] = "Reverb room size set";
            response["size"] = size;
            notifyStatusChange();
        }
        else if (action == "setReverbDecayTime") {
            float seconds = cmd.value("seconds", 2.0f);
            m_engine->setReverbDecayTime(seconds);

            // Save reverb settings for persistence
            ConfigPersistence::saveReverbSettings(m_engine->getReverbEnabled(),
                m_engine->getReverbRoomSize(), seconds, m_engine->getReverbDry(), m_engine->getReverbWet());

            response["success"] = true;
            response["message"] = "Reverb decay time set";
            response["seconds"] = seconds;
            notifyStatusChange();
        }
        else if (action == "setReverbMix") {
            float dry = cmd.value("dry", 1.0f);
            float wet = cmd.value("wet", 0.3f);
            m_engine->setReverbMix(dry, wet);

            // Save reverb settings for persistence
            ConfigPersistence::saveReverbSettings(m_engine->getReverbEnabled(),
                m_engine->getReverbRoomSize(), m_engine->getReverbDecayTime(), dry, wet);

            response["success"] = true;
            response["message"] = "Reverb mix set";
            response["dry"] = dry;
            response["wet"] = wet;
            notifyStatusChange();
        }
        // Backing track commands
        else if (action == "loadBackingTrack") {
            if (!m_backingTrack) {
                response["success"] = false;
                response["error"] = "Backing track not initialized";
            } else {
                std::string filepath = cmd.value("filepath", "");
                if (filepath.empty()) {
                    response["success"] = false;
                    response["error"] = "Missing filepath parameter";
                } else {
                    int sampleRate = (m_backend && m_backend->getSampleRate() > 0) ?
                                    m_backend->getSampleRate() : 48000;
                    bool success = m_backingTrack->loadFile(filepath, sampleRate);
                    if (success) {
                        response["success"] = true;
                        response["filename"] = m_backingTrack->getFilename();
                        response["duration"] = m_backingTrack->getDurationSeconds();
                        response["channels"] = m_backingTrack->getChannels();
                        response["sampleRate"] = m_backingTrack->getSampleRate();
                    } else {
                        response["success"] = false;
                        response["error"] = "Failed to load backing track file";
                    }
                }
            }
        }
        else if (action == "playBackingTrack") {
            if (!m_backingTrack) {
                response["success"] = false;
                response["error"] = "Backing track not initialized";
            } else {
                m_backingTrack->play();
                response["success"] = true;
            }
        }
        else if (action == "stopBackingTrack") {
            if (!m_backingTrack) {
                response["success"] = false;
                response["error"] = "Backing track not initialized";
            } else {
                m_backingTrack->stop();
                response["success"] = true;
            }
        }
        else if (action == "pauseBackingTrack") {
            if (!m_backingTrack) {
                response["success"] = false;
                response["error"] = "Backing track not initialized";
            } else {
                m_backingTrack->pause();
                response["success"] = true;
            }
        }
        else if (action == "setBackingTrackLoop") {
            if (!m_backingTrack) {
                response["success"] = false;
                response["error"] = "Backing track not initialized";
            } else {
                bool enabled = cmd.value("enabled", true);
                m_backingTrack->setLoop(enabled);
                response["success"] = true;
                response["loopEnabled"] = enabled;
            }
        }
        else if (action == "setBackingTrackVolume") {
            if (!m_backingTrack) {
                response["success"] = false;
                response["error"] = "Backing track not initialized";
            } else {
                float volume = cmd.value("volume", 0.7f);
                m_backingTrack->setVolume(volume);
                response["success"] = true;
                response["volume"] = volume;
            }
        }
        else if (action == "setIncludeBackingTrackInRecording") {
            if (!m_engine) {
                response["success"] = false;
                response["error"] = "Engine not initialized";
            } else {
                bool enabled = cmd.value("enabled", false);
                m_engine->setIncludeBackingTrackInRecording(enabled);
                response["success"] = true;
                response["enabled"] = enabled;
            }
        }
        else if (action == "getIncludeBackingTrackInRecording") {
            if (!m_engine) {
                response["success"] = false;
                response["error"] = "Engine not initialized";
            } else {
                response["success"] = true;
                response["enabled"] = m_engine->getIncludeBackingTrackInRecording();
            }
        }
        else if (action == "setBackingTrackStartPosition") {
            if (!m_backingTrack) {
                response["success"] = false;
                response["error"] = "Backing track not initialized";
            } else {
                float seconds = cmd.value("seconds", 0.0f);
                m_backingTrack->setStartPosition(seconds);
                response["success"] = true;
                response["startPosition"] = m_backingTrack->getStartPosition();
            }
        }
        else if (action == "setBackingTrackStopPosition") {
            if (!m_backingTrack) {
                response["success"] = false;
                response["error"] = "Backing track not initialized";
            } else {
                float seconds = cmd.value("seconds", 0.0f);
                m_backingTrack->setStopPosition(seconds);
                response["success"] = true;
                response["stopPosition"] = m_backingTrack->getStopPosition();
            }
        }
        else if (action == "getBackingTrackStatus") {
            if (!m_backingTrack) {
                response["success"] = false;
                response["error"] = "Backing track not initialized";
            } else {
                response["success"] = true;
                response["loaded"] = m_backingTrack->isLoaded();
                response["playing"] = m_backingTrack->isPlaying();
                response["looping"] = m_backingTrack->isLooping();
                response["volume"] = m_backingTrack->getVolume();
                if (m_backingTrack->isLoaded()) {
                    response["filename"] = m_backingTrack->getFilename();
                    response["duration"] = m_backingTrack->getDurationSeconds();
                    response["position"] = (float)m_backingTrack->getCurrentFrame() /
                                          m_backingTrack->getSampleRate();
                    response["channels"] = m_backingTrack->getChannels();
                    response["sampleRate"] = m_backingTrack->getSampleRate();
                    response["startPosition"] = m_backingTrack->getStartPosition();
                    response["stopPosition"] = m_backingTrack->getStopPosition();
                }
            }
        }
        else {
            response["success"] = false;
            response["error"] = "Unknown action: " + action;
        }

    } catch (const json::exception& e) {
        response["success"] = false;
        response["error"] = std::string("JSON parse error: ") + e.what();
    }

    return response.dump();
}

} // namespace HoopiPi
