#pragma once

#include <string>
#include <fstream>
#include <filesystem>
#include "../../dependencies/NeuralAudio/deps/NeuralAmpModelerCore/Dependencies/nlohmann/json.hpp"

namespace HoopiPi {

/**
 * @brief Handles persistence of runtime configuration
 *
 * Saves and loads the last loaded model and settings so they
 * persist across engine restarts.
 */
class ConfigPersistence {
public:
    /**
     * @brief Load runtime configuration from file
     * @param configPath Path to config file (default: ~/.config/hoopi-pi/runtime.json)
     * @return JSON object with configuration, or empty object if file doesn't exist
     */
    static nlohmann::json load(const std::string& configPath = getDefaultConfigPath()) {
        nlohmann::json config = nlohmann::json::object();

        try {
            if (std::filesystem::exists(configPath)) {
                std::ifstream file(configPath);
                if (file.is_open()) {
                    file >> config;
                    file.close();
                }
            }
        } catch (const std::exception& e) {
            // If config doesn't exist or is corrupted, return empty config
            config = nlohmann::json::object();
        }

        // Ensure it's always an object, not null
        if (config.is_null()) {
            config = nlohmann::json::object();
        }

        return config;
    }

    /**
     * @brief Save runtime configuration to file
     * @param config JSON configuration to save
     * @param configPath Path to config file (default: ~/.config/hoopi-pi/runtime.json)
     * @return true if saved successfully
     */
    static bool save(const nlohmann::json& config, const std::string& configPath = getDefaultConfigPath()) {
        try {
            // Ensure directory exists
            std::filesystem::path path(configPath);
            std::filesystem::create_directories(path.parent_path());

            // Write config
            std::ofstream file(configPath);
            if (!file.is_open()) {
                return false;
            }

            file << config.dump(2);
            file.close();

            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }

    /**
     * @brief Get default config file path
     */
    static std::string getDefaultConfigPath() {
        const char* home = getenv("HOME");
        if (home) {
            return std::string(home) + "/.config/hoopi-pi/runtime.json";
        }
        return "/tmp/hoopi-pi-runtime.json";
    }

    /**
     * @brief Save model configuration for a specific slot
     */
    static bool saveModelConfig(const std::string& modelPath, int slot) {
        nlohmann::json config = load();
        std::string slotKey = "slot" + std::to_string(slot) + "Model";
        config[slotKey] = modelPath;
        return save(config);
    }

    /**
     * @brief Save active slot
     */
    static bool saveActiveSlot(int slot) {
        nlohmann::json config = load();
        config["activeSlot"] = slot;
        return save(config);
    }

    /**
     * @brief Get model path for a specific slot
     */
    static std::string getSlotModelPath(int slot) {
        nlohmann::json config = load();
        std::string slotKey = "slot" + std::to_string(slot) + "Model";
        return config.value(slotKey, "");
    }

    /**
     * @brief Get last active slot
     */
    static int getActiveSlot() {
        nlohmann::json config = load();
        return config.value("activeSlot", 0);
    }

    /**
     * @brief Clear saved model configuration for a slot
     */
    static bool clearSlotConfig(int slot) {
        nlohmann::json config = load();
        std::string slotKey = "slot" + std::to_string(slot) + "Model";
        config.erase(slotKey);
        return save(config);
    }

    /**
     * @brief Get last loaded model path (deprecated - use getSlotModelPath)
     */
    static std::string getLastModelPath() {
        return getSlotModelPath(0);
    }

    /**
     * @brief Get last active slot (deprecated - use getActiveSlot)
     */
    static int getLastSlot() {
        return getActiveSlot();
    }

    /**
     * @brief Save gain settings
     */
    static bool saveGainSettings(float inputGain, float outputGain) {
        nlohmann::json config = load();
        config["inputGain"] = inputGain;
        config["outputGain"] = outputGain;
        return save(config);
    }

    /**
     * @brief Get saved input gain
     */
    static float getInputGain(float defaultValue = 0.0f) {
        nlohmann::json config = load();
        return config.value("inputGain", defaultValue);
    }

    /**
     * @brief Get saved output gain
     */
    static float getOutputGain(float defaultValue = 0.0f) {
        nlohmann::json config = load();
        return config.value("outputGain", defaultValue);
    }

    /**
     * @brief Save EQ settings
     */
    static bool saveEQSettings(bool enabled, float bass, float mid, float treble) {
        nlohmann::json config = load();
        config["eqEnabled"] = enabled;
        config["eqBass"] = bass;
        config["eqMid"] = mid;
        config["eqTreble"] = treble;
        return save(config);
    }

    /**
     * @brief Get saved EQ enabled state
     */
    static bool getEQEnabled(bool defaultValue = false) {
        nlohmann::json config = load();
        return config.value("eqEnabled", defaultValue);
    }

    /**
     * @brief Get saved EQ bass
     */
    static float getEQBass(float defaultValue = 0.0f) {
        nlohmann::json config = load();
        return config.value("eqBass", defaultValue);
    }

    /**
     * @brief Get saved EQ mid
     */
    static float getEQMid(float defaultValue = 0.0f) {
        nlohmann::json config = load();
        return config.value("eqMid", defaultValue);
    }

    /**
     * @brief Get saved EQ treble
     */
    static float getEQTreble(float defaultValue = 0.0f) {
        nlohmann::json config = load();
        return config.value("eqTreble", defaultValue);
    }

    /**
     * @brief Save noise gate settings
     */
    static bool saveNoiseGateSettings(bool enabled, float threshold) {
        nlohmann::json config = load();
        config["noiseGateEnabled"] = enabled;
        config["noiseGateThreshold"] = threshold;
        return save(config);
    }

    /**
     * @brief Get saved noise gate enabled state
     */
    static bool getNoiseGateEnabled(bool defaultValue = false) {
        nlohmann::json config = load();
        return config.value("noiseGateEnabled", defaultValue);
    }

    /**
     * @brief Get saved noise gate threshold
     */
    static float getNoiseGateThreshold(float defaultValue = -40.0f) {
        nlohmann::json config = load();
        return config.value("noiseGateThreshold", defaultValue);
    }

    /**
     * @brief Save reverb settings
     */
    static bool saveReverbSettings(bool enabled, float roomSize, float decayTime, float dry, float wet) {
        nlohmann::json config = load();
        config["reverbEnabled"] = enabled;
        config["reverbRoomSize"] = roomSize;
        config["reverbDecayTime"] = decayTime;
        config["reverbDry"] = dry;
        config["reverbWet"] = wet;
        return save(config);
    }

    /**
     * @brief Get saved reverb enabled state
     */
    static bool getReverbEnabled(bool defaultValue = false) {
        nlohmann::json config = load();
        return config.value("reverbEnabled", defaultValue);
    }

    /**
     * @brief Get saved reverb room size
     */
    static float getReverbRoomSize(float defaultValue = 0.3f) {
        nlohmann::json config = load();
        return config.value("reverbRoomSize", defaultValue);
    }

    /**
     * @brief Get saved reverb decay time
     */
    static float getReverbDecayTime(float defaultValue = 2.0f) {
        nlohmann::json config = load();
        return config.value("reverbDecayTime", defaultValue);
    }

    /**
     * @brief Get saved reverb dry level
     */
    static float getReverbDry(float defaultValue = 1.0f) {
        nlohmann::json config = load();
        return config.value("reverbDry", defaultValue);
    }

    /**
     * @brief Get saved reverb wet level
     */
    static float getReverbWet(float defaultValue = 0.3f) {
        nlohmann::json config = load();
        return config.value("reverbWet", defaultValue);
    }

    // ===== Stereo Mode =====

    /**
     * @brief Save stereo mode
     */
    static bool saveStereoMode(const std::string& mode) {
        nlohmann::json config = load();
        config["stereoMode"] = mode;
        return save(config);
    }

    /**
     * @brief Get saved stereo mode
     */
    static std::string getStereoMode(const std::string& defaultValue = "LeftMono2Stereo") {
        nlohmann::json config = load();
        return config.value("stereoMode", defaultValue);
    }

    // ===== Per-Channel Gains =====

    /**
     * @brief Save per-channel gain settings
     */
    static bool savePerChannelGains(float inputGainL, float inputGainR, float outputGainL, float outputGainR) {
        nlohmann::json config = load();
        config["inputGainL"] = inputGainL;
        config["inputGainR"] = inputGainR;
        config["outputGainL"] = outputGainL;
        config["outputGainR"] = outputGainR;
        return save(config);
    }

    static float getInputGainL(float defaultValue = 0.0f) {
        nlohmann::json config = load();
        return config.value("inputGainL", defaultValue);
    }

    static float getInputGainR(float defaultValue = 0.0f) {
        nlohmann::json config = load();
        return config.value("inputGainR", defaultValue);
    }

    static float getOutputGainL(float defaultValue = 0.0f) {
        nlohmann::json config = load();
        return config.value("outputGainL", defaultValue);
    }

    static float getOutputGainR(float defaultValue = 0.0f) {
        nlohmann::json config = load();
        return config.value("outputGainR", defaultValue);
    }

    // ===== Per-Channel EQ =====

    /**
     * @brief Save per-channel EQ settings
     */
    static bool savePerChannelEQ(bool enabledL, float bassL, float midL, float trebleL,
                                   bool enabledR, float bassR, float midR, float trebleR) {
        nlohmann::json config = load();
        config["eqEnabledL"] = enabledL;
        config["eqBassL"] = bassL;
        config["eqMidL"] = midL;
        config["eqTrebleL"] = trebleL;
        config["eqEnabledR"] = enabledR;
        config["eqBassR"] = bassR;
        config["eqMidR"] = midR;
        config["eqTrebleR"] = trebleR;
        return save(config);
    }

    static bool getEQEnabledL(bool defaultValue = false) {
        nlohmann::json config = load();
        return config.value("eqEnabledL", defaultValue);
    }

    static bool getEQEnabledR(bool defaultValue = false) {
        nlohmann::json config = load();
        return config.value("eqEnabledR", defaultValue);
    }

    static float getEQBassL(float defaultValue = 0.0f) {
        nlohmann::json config = load();
        return config.value("eqBassL", defaultValue);
    }

    static float getEQMidL(float defaultValue = 0.0f) {
        nlohmann::json config = load();
        return config.value("eqMidL", defaultValue);
    }

    static float getEQTrebleL(float defaultValue = 0.0f) {
        nlohmann::json config = load();
        return config.value("eqTrebleL", defaultValue);
    }

    static float getEQBassR(float defaultValue = 0.0f) {
        nlohmann::json config = load();
        return config.value("eqBassR", defaultValue);
    }

    static float getEQMidR(float defaultValue = 0.0f) {
        nlohmann::json config = load();
        return config.value("eqMidR", defaultValue);
    }

    static float getEQTrebleR(float defaultValue = 0.0f) {
        nlohmann::json config = load();
        return config.value("eqTrebleR", defaultValue);
    }

    // ===== Per-Channel Noise Gate =====

    /**
     * @brief Save per-channel noise gate settings
     */
    static bool savePerChannelNoiseGate(bool enabledL, float thresholdL, bool enabledR, float thresholdR) {
        nlohmann::json config = load();
        config["noiseGateEnabledL"] = enabledL;
        config["noiseGateThresholdL"] = thresholdL;
        config["noiseGateEnabledR"] = enabledR;
        config["noiseGateThresholdR"] = thresholdR;
        return save(config);
    }

    static bool getNoiseGateEnabledL(bool defaultValue = false) {
        nlohmann::json config = load();
        return config.value("noiseGateEnabledL", defaultValue);
    }

    static bool getNoiseGateEnabledR(bool defaultValue = false) {
        nlohmann::json config = load();
        return config.value("noiseGateEnabledR", defaultValue);
    }

    static float getNoiseGateThresholdL(float defaultValue = -40.0f) {
        nlohmann::json config = load();
        return config.value("noiseGateThresholdL", defaultValue);
    }

    static float getNoiseGateThresholdR(float defaultValue = -40.0f) {
        nlohmann::json config = load();
        return config.value("noiseGateThresholdR", defaultValue);
    }

    // ===== NAM Bypass States =====

    /**
     * @brief Save NAM bypass states
     */
    static bool saveBypassStates(bool bypassL, bool bypassR) {
        nlohmann::json config = load();
        config["bypassModelL"] = bypassL;
        config["bypassModelR"] = bypassR;
        return save(config);
    }

    static bool getBypassModelL(bool defaultValue = false) {
        nlohmann::json config = load();
        return config.value("bypassModelL", defaultValue);
    }

    static bool getBypassModelR(bool defaultValue = false) {
        nlohmann::json config = load();
        return config.value("bypassModelR", defaultValue);
    }

    // ===== Stereo2Mono Mix Levels =====

    /**
     * @brief Save stereo2mono mix levels
     */
    static bool saveStereo2MonoMix(float mixL, float mixR) {
        nlohmann::json config = load();
        config["stereo2MonoMixL"] = mixL;
        config["stereo2MonoMixR"] = mixR;
        return save(config);
    }

    static float getStereo2MonoMixL(float defaultValue = 0.0f) {
        nlohmann::json config = load();
        return config.value("stereo2MonoMixL", defaultValue);
    }

    static float getStereo2MonoMixR(float defaultValue = 0.0f) {
        nlohmann::json config = load();
        return config.value("stereo2MonoMixR", defaultValue);
    }
};

} // namespace HoopiPi
