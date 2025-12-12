#include "Engine.h"
#include "JackBackend.h"
#include "IPCServer.h"
#include "ConfigPersistence.h"
#include "BackingTrack.h"
#include <iostream>
#include <csignal>
#include <cstring>
#include <atomic>
#include <thread>
#include <chrono>
#include <filesystem>

// Global flag for signal handling
static std::atomic<bool> g_running{true};

// Global backing track instance
static HoopiPi::BackingTrack g_backingTrack;

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    g_running.store(false);
}

void printUsage(const char* progName) {
    std::cout << "HoopiPi - Headless Neural Audio Processor (JACK)\n\n";
    std::cout << "Usage: " << progName << " [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --model PATH         Model file to load (.nam or .json)\n";
    std::cout << "  --client-name NAME   JACK client name (default: HoopiPi)\n";
    std::cout << "  --no-auto-connect    Don't auto-connect to system ports\n";
    std::cout << "  --input-gain DB      Input gain in dB (default: 0.0)\n";
    std::cout << "  --output-gain DB     Output gain in dB (default: 0.0)\n";
    std::cout << "  --bypass             Start in bypass mode\n";
    std::cout << "  --no-normalize       Disable output normalization\n";
    std::cout << "  --gate-threshold DB  Noise gate threshold in dB (default: -40.0)\n";
    std::cout << "  --enable-gate        Enable noise gate\n";
    std::cout << "  --enable-dc-blocker  Enable DC blocking filter\n";
    std::cout << "  --help               Show this help message\n";
    std::cout << std::endl;
}

void printStatus(HoopiPi::Engine* engine, HoopiPi::JackBackend* backend) {
    std::cout << "\n=== HoopiPi Status ===" << std::endl;

    // JACK connection status
    HoopiPi::JackStatus status = backend->getStatus();
    std::cout << "JACK Status:    ";
    switch (status) {
        case HoopiPi::JackStatus::Connected:
            std::cout << "Connected" << std::endl;
            break;
        case HoopiPi::JackStatus::Connecting:
            std::cout << "Connecting..." << std::endl;
            break;
        case HoopiPi::JackStatus::Disconnected:
            std::cout << "Disconnected" << std::endl;
            break;
        case HoopiPi::JackStatus::Error:
            std::cout << "Error: " << backend->getErrorMessage() << std::endl;
            break;
    }

    if (status == HoopiPi::JackStatus::Connected) {
        std::cout << "Sample Rate:    " << backend->getSampleRate() << " Hz" << std::endl;
        std::cout << "Buffer Size:    " << backend->getBufferSize() << " frames" << std::endl;
        std::cout << "Latency:        " << backend->getLatencyMs() << " ms" << std::endl;
        std::cout << "CPU Load:       " << backend->getCPULoad() << "%" << std::endl;
    }

    std::cout << "Active Model:   Slot " << engine->getActiveModel() << std::endl;

    // Always show L/R settings (use L for mono processing)
    bool isTrueStereo = (engine->getStereoMode() == HoopiPi::StereoMode::Stereo2Stereo);

    if (isTrueStereo) {
        std::cout << "Stereo Mode:    True Stereo" << std::endl;
    } else {
        std::cout << "Stereo Mode:    Mono (using L settings)" << std::endl;
    }

    std::cout << "Input Gain L:   " << engine->getInputGainL() << " dB" << std::endl;
    if (isTrueStereo) {
        std::cout << "Input Gain R:   " << engine->getInputGainR() << " dB" << std::endl;
    }
    std::cout << "Output Gain L:  " << engine->getOutputGainL() << " dB" << std::endl;
    if (isTrueStereo) {
        std::cout << "Output Gain R:  " << engine->getOutputGainR() << " dB" << std::endl;
    }
    std::cout << "Bypass NAM L:   " << (engine->getBypassModelL() ? "ON" : "OFF") << std::endl;
    if (isTrueStereo) {
        std::cout << "Bypass NAM R:   " << (engine->getBypassModelR() ? "ON" : "OFF") << std::endl;
    }
    std::cout << "Noise Gate L:   " << (engine->getNoiseGateEnabledL() ? "ON" : "OFF");
    if (engine->getNoiseGateEnabledL()) {
        std::cout << " (" << engine->getNoiseGateThresholdL() << " dB)";
    }
    std::cout << std::endl;
    if (isTrueStereo) {
        std::cout << "Noise Gate R:   " << (engine->getNoiseGateEnabledR() ? "ON" : "OFF");
        if (engine->getNoiseGateEnabledR()) {
            std::cout << " (" << engine->getNoiseGateThresholdR() << " dB)";
        }
        std::cout << std::endl;
    }

    std::cout << "Normalize:      " << (engine->getNormalize() ? "ON" : "OFF") << std::endl;
    std::cout << "DC Blocker:     " << (engine->getDCBlockerEnabled() ? "ON" : "OFF") << std::endl;
    std::cout << "Xruns:          " << backend->getXrunCount() << std::endl;
    std::cout << "======================" << std::endl;
}

int main(int argc, char* argv[]) {
    // Default parameters
    const char* modelPath = nullptr;
    const char* clientName = "HoopiPi";
    bool autoConnect = true;
    float inputGain = 0.0f;
    float outputGain = 0.0f;
    bool bypass = false;
    bool normalize = true;
    float gateThreshold = -40.0f;
    bool enableGate = false;
    bool enableDCBlocker = false;

    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--model") == 0 && i + 1 < argc) {
            modelPath = argv[++i];
        } else if (strcmp(argv[i], "--client-name") == 0 && i + 1 < argc) {
            clientName = argv[++i];
        } else if (strcmp(argv[i], "--no-auto-connect") == 0) {
            autoConnect = false;
        } else if (strcmp(argv[i], "--input-gain") == 0 && i + 1 < argc) {
            inputGain = atof(argv[++i]);
        } else if (strcmp(argv[i], "--output-gain") == 0 && i + 1 < argc) {
            outputGain = atof(argv[++i]);
        } else if (strcmp(argv[i], "--bypass") == 0) {
            bypass = true;
        } else if (strcmp(argv[i], "--no-normalize") == 0) {
            normalize = false;
        } else if (strcmp(argv[i], "--gate-threshold") == 0 && i + 1 < argc) {
            gateThreshold = atof(argv[++i]);
        } else if (strcmp(argv[i], "--enable-gate") == 0) {
            enableGate = true;
        } else if (strcmp(argv[i], "--enable-dc-blocker") == 0) {
            enableDCBlocker = true;
        } else {
            std::cerr << "Unknown option: " << argv[i] << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    // Setup signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::cout << "HoopiPi - Headless Neural Audio Processor (JACK)" << std::endl;
    std::cout << "==================================================" << std::endl;

    // Create engine (will get actual buffer size from JACK)
    std::cout << "Creating engine..." << std::endl;
    HoopiPi::Engine engine(48000, 1024); // Large buffer, will be adjusted by JACK
    if (!engine.init()) {
        std::cerr << "Failed to initialize engine" << std::endl;
        return 1;
    }

    // Set parameters (load from config if not specified via command line)
    float savedInputGain = HoopiPi::ConfigPersistence::getInputGain(inputGain);
    float savedOutputGain = HoopiPi::ConfigPersistence::getOutputGain(outputGain);

    engine.setInputGain(savedInputGain);
    engine.setOutputGain(savedOutputGain);
    engine.setNormalize(normalize);
    engine.setNoiseGate(enableGate, gateThreshold);
    engine.setDCBlocker(enableDCBlocker);

    // Load EQ settings from config
    bool eqEnabled = HoopiPi::ConfigPersistence::getEQEnabled(false);
    float eqBass = HoopiPi::ConfigPersistence::getEQBass(0.0f);
    float eqMid = HoopiPi::ConfigPersistence::getEQMid(0.0f);
    float eqTreble = HoopiPi::ConfigPersistence::getEQTreble(0.0f);

    engine.setEQEnabled(eqEnabled);
    engine.setEQBass(eqBass);
    engine.setEQMid(eqMid);
    engine.setEQTreble(eqTreble);

    // Load noise gate settings from config
    bool noiseGateEnabled = HoopiPi::ConfigPersistence::getNoiseGateEnabled(false);
    float noiseGateThreshold = HoopiPi::ConfigPersistence::getNoiseGateThreshold(-40.0f);
    engine.setNoiseGate(noiseGateEnabled, noiseGateThreshold);

    // Load reverb settings from config
    bool reverbEnabled = HoopiPi::ConfigPersistence::getReverbEnabled(false);
    float reverbRoomSize = HoopiPi::ConfigPersistence::getReverbRoomSize(0.3f);
    float reverbDecayTime = HoopiPi::ConfigPersistence::getReverbDecayTime(2.0f);
    float reverbDry = HoopiPi::ConfigPersistence::getReverbDry(1.0f);
    float reverbWet = HoopiPi::ConfigPersistence::getReverbWet(0.3f);

    engine.setReverbEnabled(reverbEnabled);
    engine.setReverbRoomSize(reverbRoomSize);
    engine.setReverbDecayTime(reverbDecayTime);
    engine.setReverbMix(reverbDry, reverbWet);

    // Load stereo mode from config
    std::string stereoModeStr = HoopiPi::ConfigPersistence::getStereoMode("LeftMono2Stereo");
    HoopiPi::StereoMode stereoMode = HoopiPi::StereoMode::LeftMono2Stereo;
    if (stereoModeStr == "Stereo2Stereo") {
        stereoMode = HoopiPi::StereoMode::Stereo2Stereo;
    } else if (stereoModeStr == "RightMono2Stereo") {
        stereoMode = HoopiPi::StereoMode::RightMono2Stereo;
    } else if (stereoModeStr == "Stereo2Mono") {
        stereoMode = HoopiPi::StereoMode::Stereo2Mono;
    }
    engine.setStereoMode(stereoMode);

    // Load per-channel gain settings from config
    float inputGainL = HoopiPi::ConfigPersistence::getInputGainL(0.0f);
    float inputGainR = HoopiPi::ConfigPersistence::getInputGainR(0.0f);
    float outputGainL = HoopiPi::ConfigPersistence::getOutputGainL(0.0f);
    float outputGainR = HoopiPi::ConfigPersistence::getOutputGainR(0.0f);
    engine.setInputGainL(inputGainL);
    engine.setInputGainR(inputGainR);
    engine.setOutputGainL(outputGainL);
    engine.setOutputGainR(outputGainR);

    // Load per-channel EQ settings from config
    bool eqEnabledL = HoopiPi::ConfigPersistence::getEQEnabledL(false);
    float eqBassL = HoopiPi::ConfigPersistence::getEQBassL(0.0f);
    float eqMidL = HoopiPi::ConfigPersistence::getEQMidL(0.0f);
    float eqTrebleL = HoopiPi::ConfigPersistence::getEQTrebleL(0.0f);
    bool eqEnabledR = HoopiPi::ConfigPersistence::getEQEnabledR(false);
    float eqBassR = HoopiPi::ConfigPersistence::getEQBassR(0.0f);
    float eqMidR = HoopiPi::ConfigPersistence::getEQMidR(0.0f);
    float eqTrebleR = HoopiPi::ConfigPersistence::getEQTrebleR(0.0f);
    engine.setEQEnabledL(eqEnabledL);
    engine.setEQBassL(eqBassL);
    engine.setEQMidL(eqMidL);
    engine.setEQTrebleL(eqTrebleL);
    engine.setEQEnabledR(eqEnabledR);
    engine.setEQBassR(eqBassR);
    engine.setEQMidR(eqMidR);
    engine.setEQTrebleR(eqTrebleR);

    // Load per-channel noise gate settings from config
    // If per-channel settings don't exist, use legacy settings as default
    bool noiseGateEnabledL = HoopiPi::ConfigPersistence::getNoiseGateEnabledL(noiseGateEnabled);
    float noiseGateThresholdL = HoopiPi::ConfigPersistence::getNoiseGateThresholdL(noiseGateThreshold);
    bool noiseGateEnabledR = HoopiPi::ConfigPersistence::getNoiseGateEnabledR(noiseGateEnabled);
    float noiseGateThresholdR = HoopiPi::ConfigPersistence::getNoiseGateThresholdR(noiseGateThreshold);
    engine.setNoiseGateL(noiseGateEnabledL, noiseGateThresholdL);
    engine.setNoiseGateR(noiseGateEnabledR, noiseGateThresholdR);

    // Save per-channel settings to persist the migration
    HoopiPi::ConfigPersistence::savePerChannelNoiseGate(noiseGateEnabledL, noiseGateThresholdL,
                                                         noiseGateEnabledR, noiseGateThresholdR);

    // Load NAM bypass states from config
    bool bypassModelL = HoopiPi::ConfigPersistence::getBypassModelL(false);
    bool bypassModelR = HoopiPi::ConfigPersistence::getBypassModelR(false);
    engine.setBypassModelL(bypassModelL);
    engine.setBypassModelR(bypassModelR);

    // Load Stereo2Mono mix levels from config
    float stereo2MonoMixL = HoopiPi::ConfigPersistence::getStereo2MonoMixL(0.0f);
    float stereo2MonoMixR = HoopiPi::ConfigPersistence::getStereo2MonoMixR(0.0f);
    engine.setStereo2MonoMixL(stereo2MonoMixL);
    engine.setStereo2MonoMixR(stereo2MonoMixR);

    // Load models from saved configuration (unless overridden by command line)
    bool anyModelLoaded = false;

    if (modelPath) {
        // Command line model overrides saved config
        std::cout << "Loading model from command line: " << modelPath << std::endl;
        engine.loadModelAsync(0, modelPath);

        // Wait for model to load (with timeout)
        int waitCount = 0;
        while (!engine.isModelReady(0) && waitCount < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            waitCount++;
        }

        if (engine.isModelReady(0)) {
            std::cout << "Successfully loaded command line model into slot 0" << std::endl;
            anyModelLoaded = true;
        } else {
            std::cerr << "Warning: Model loading timed out" << std::endl;
        }
    } else {
        // Load both slots from saved config
        for (int slot = 0; slot < 2; slot++) {
            std::string slotModel = HoopiPi::ConfigPersistence::getSlotModelPath(slot);
            if (!slotModel.empty() && std::filesystem::exists(slotModel)) {
                std::cout << "Loading saved model into slot " << slot << ": " << slotModel << std::endl;
                engine.loadModelAsync(slot, slotModel);

                // Wait for model to load (with timeout)
                int waitCount = 0;
                while (!engine.isModelReady(slot) && waitCount < 100) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    waitCount++;
                }

                if (engine.isModelReady(slot)) {
                    std::cout << "Successfully loaded model into slot " << slot << std::endl;
                    anyModelLoaded = true;
                } else {
                    std::cerr << "Warning: Failed to load model into slot " << slot << std::endl;
                }
            }
        }

        // Set the active slot from config
        if (anyModelLoaded) {
            int activeSlot = HoopiPi::ConfigPersistence::getActiveSlot();
            if (engine.isModelReady(activeSlot)) {
                engine.setActiveModel(activeSlot);
                std::cout << "Set active slot to " << activeSlot << std::endl;
            } else {
                // Active slot model didn't load, try the other slot
                int otherSlot = (activeSlot == 0) ? 1 : 0;
                if (engine.isModelReady(otherSlot)) {
                    engine.setActiveModel(otherSlot);
                    std::cout << "Active slot model not ready, using slot " << otherSlot << std::endl;
                }
            }
        }
    }

    // Set bypass mode based on whether we have any models loaded
    if (anyModelLoaded) {
        engine.setBypass(false);
    } else {
        std::cout << "No models loaded - starting in bypass mode" << std::endl;
        std::cout << "Use the web interface to load a model" << std::endl;
        engine.setBypass(true);
    }

    // Create JACK backend
    std::cout << "Creating JACK backend..." << std::endl;
    HoopiPi::JackBackend backend(&engine);

    // Set backing track for audio mixing and optional recording
    backend.setBackingTrack(&g_backingTrack);
    engine.setBackingTrack(&g_backingTrack);

    bool jackConnected = false;
    if (!backend.init(clientName, autoConnect)) {
        std::cerr << "Failed to initialize JACK backend" << std::endl;
        std::cerr << "Error: " << backend.getErrorMessage() << std::endl;
        std::cerr << "Continuing in degraded mode - will retry connection..." << std::endl;
    } else {
        // Start audio processing
        std::cout << "Starting JACK audio processing..." << std::endl;
        if (!backend.start()) {
            std::cerr << "Failed to start JACK backend" << std::endl;
            std::cerr << "Error: " << backend.getErrorMessage() << std::endl;
            std::cerr << "Continuing in degraded mode - will retry connection..." << std::endl;
        } else {
            jackConnected = true;
        }
    }

    // Start IPC server
    std::cout << "Starting IPC server..." << std::endl;
    HoopiPi::IPCServer ipcServer(&engine);
    ipcServer.setBackend(&backend);  // Pass backend for CPU load queries
    ipcServer.setBackingTrack(&g_backingTrack);  // Pass backing track for control

    // Set callback to print status when settings change via IPC
    ipcServer.setStatusChangeCallback([&engine, &backend]() {
        printStatus(&engine, &backend);
    });

    if (!ipcServer.start()) {
        std::cerr << "Warning: Failed to start IPC server" << std::endl;
    }

    if (jackConnected) {
        std::cout << "\nAudio processing started successfully!" << std::endl;
    } else {
        std::cout << "\nRunning in degraded mode (JACK not connected)" << std::endl;
        std::cout << "Will attempt to reconnect every 5 seconds..." << std::endl;
    }
    std::cout << "Press Ctrl+C to stop...\n" << std::endl;

    // Print initial status
    if (jackConnected) {
        printStatus(&engine, &backend);
    }

    // Main loop - monitor for errors and connection issues
    auto lastRetryTime = std::chrono::steady_clock::now();
    uint32_t lastXrunCount = 0;
    const int RETRY_INTERVAL_SECONDS = 5;

    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        // Check JACK connection status
        HoopiPi::JackStatus status = backend.getStatus();

        if (status != HoopiPi::JackStatus::Connected) {
            // Not connected - try to reconnect periodically
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastRetryTime);

            if (elapsed.count() >= RETRY_INTERVAL_SECONDS) {
                std::cout << "Attempting to reconnect to JACK..." << std::endl;
                if (backend.reconnect(clientName, autoConnect)) {
                    std::cout << "Successfully reconnected to JACK!" << std::endl;
                    jackConnected = true;
                    printStatus(&engine, &backend);
                } else {
                    std::cerr << "Reconnection failed: " << backend.getErrorMessage() << std::endl;
                    std::cerr << "Will retry in " << RETRY_INTERVAL_SECONDS << " seconds..." << std::endl;
                }
                lastRetryTime = now;
            }
        } else if (jackConnected) {
            // Connected and working
            // Check for xruns and print error when detected
            uint32_t currentXruns = backend.getXrunCount();
            if (currentXruns != lastXrunCount) {
                std::cerr << "Xrun detected! Total: " << currentXruns << std::endl;
                lastXrunCount = currentXruns;
            }
        }
    }

    // Cleanup
    std::cout << "\nStopping IPC server..." << std::endl;
    ipcServer.stop();

    std::cout << "Stopping JACK..." << std::endl;
    backend.stop();

    std::cout << "Cleaning up..." << std::endl;
    engine.cleanup();

    std::cout << "Shutdown complete" << std::endl;

    return 0;
}
