#include "Engine.h"
#include "JackBackend.h"
#include <iostream>
#include <csignal>
#include <cstring>
#include <atomic>
#include <thread>
#include <chrono>

// Global flag for signal handling
static std::atomic<bool> g_running{true};

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

void modelLoadCallback(int slot, int success, const char* error, void* userData) {
    if (success) {
        std::cout << "Model loaded successfully in slot " << slot << std::endl;
    } else {
        std::cerr << "Model load failed in slot " << slot << ": " << error << std::endl;
    }
}

void printStatus(HoopiPi::Engine* engine, HoopiPi::JackBackend* backend) {
    std::cout << "\n=== HoopiPi Status ===" << std::endl;
    std::cout << "Sample Rate:    " << backend->getSampleRate() << " Hz" << std::endl;
    std::cout << "Buffer Size:    " << backend->getBufferSize() << " frames" << std::endl;
    std::cout << "Latency:        " << backend->getLatencyMs() << " ms" << std::endl;
    std::cout << "Active Model:   Slot " << engine->getActiveModel() << std::endl;
    std::cout << "Input Gain:     " << engine->getInputGain() << " dB" << std::endl;
    std::cout << "Output Gain:    " << engine->getOutputGain() << " dB" << std::endl;
    std::cout << "Bypass:         " << (engine->getBypass() ? "ON" : "OFF") << std::endl;
    std::cout << "Normalize:      " << (engine->getNormalize() ? "ON" : "OFF") << std::endl;
    std::cout << "Noise Gate:     " << (engine->getNoiseGateEnabled() ? "ON" : "OFF");
    if (engine->getNoiseGateEnabled()) {
        std::cout << " (" << engine->getNoiseGateThreshold() << " dB)";
    }
    std::cout << std::endl;
    std::cout << "DC Blocker:     " << (engine->getDCBlockerEnabled() ? "ON" : "OFF") << std::endl;
    std::cout << "Xruns:          " << backend->getXrunCount() << std::endl;
    std::cout << "======================" << std::endl;
}

int main(int argc, char* argv[]) {
    // Default parameters
    const char* deviceName = "hw:DaisySeed";
    const char* modelPath = nullptr;
    int sampleRate = 48000;
    int periodSize = 128;
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
        } else if (strcmp(argv[i], "--device") == 0 && i + 1 < argc) {
            deviceName = argv[++i];
        } else if (strcmp(argv[i], "--model") == 0 && i + 1 < argc) {
            modelPath = argv[++i];
        } else if (strcmp(argv[i], "--sample-rate") == 0 && i + 1 < argc) {
            sampleRate = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--period-size") == 0 && i + 1 < argc) {
            periodSize = atoi(argv[++i]);
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

    std::cout << "HoopiPi - Headless Neural Audio Processor" << std::endl;
    std::cout << "==========================================" << std::endl;

    // Create engine
    std::cout << "Creating engine..." << std::endl;
    HoopiPi_Engine* engine = HoopiPi_CreateEngine(sampleRate, periodSize * 2);
    if (!engine) {
        std::cerr << "Failed to create engine" << std::endl;
        return 1;
    }

    // Initialize engine
    if (!HoopiPi_Init(engine)) {
        std::cerr << "Failed to initialize engine" << std::endl;
        HoopiPi_DeleteEngine(engine);
        return 1;
    }

    // Set model load callback
    HoopiPi_SetModelLoadCallback(engine, modelLoadCallback, nullptr);

    // Set parameters
    HoopiPi_SetInputGain(engine, inputGain);
    HoopiPi_SetOutputGain(engine, outputGain);
    HoopiPi_SetBypass(engine, bypass ? 1 : 0);
    HoopiPi_SetNormalize(engine, normalize ? 1 : 0);
    HoopiPi_SetNoiseGate(engine, enableGate ? 1 : 0, gateThreshold);
    HoopiPi_SetDCBlocker(engine, enableDCBlocker ? 1 : 0);

    // Load model if specified
    if (modelPath) {
        std::cout << "Loading model: " << modelPath << std::endl;
        HoopiPi_LoadModelAsync(engine, 0, modelPath);

        // Wait for model to load (with timeout)
        int waitCount = 0;
        while (!HoopiPi_IsModelReady(engine, 0) && waitCount < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            waitCount++;
        }

        if (!HoopiPi_IsModelReady(engine, 0)) {
            std::cerr << "Warning: Model loading timed out" << std::endl;
        }
    }

    // Create ALSA backend
    std::cout << "Creating ALSA backend..." << std::endl;
    HoopiPi_Backend* backend = HoopiPi_CreateAlsaBackend(engine);
    if (!backend) {
        std::cerr << "Failed to create ALSA backend" << std::endl;
        HoopiPi_DeleteEngine(engine);
        return 1;
    }

    // Initialize ALSA
    std::cout << "Initializing ALSA device: " << deviceName << std::endl;
    if (!HoopiPi_InitAlsa(backend, deviceName, sampleRate, periodSize, 2)) {
        std::cerr << "Failed to initialize ALSA" << std::endl;
        HoopiPi_DeleteAlsaBackend(backend);
        HoopiPi_DeleteEngine(engine);
        return 1;
    }

    // Start audio
    std::cout << "Starting audio processing..." << std::endl;
    if (!HoopiPi_StartAudio(backend)) {
        std::cerr << "Failed to start audio" << std::endl;
        HoopiPi_DeleteAlsaBackend(backend);
        HoopiPi_DeleteEngine(engine);
        return 1;
    }

    std::cout << "\nAudio processing started successfully!" << std::endl;
    std::cout << "Press Ctrl+C to stop...\n" << std::endl;

    // Print initial status
    printStatus(engine, backend);

    // Main loop - monitor and print status periodically
    auto lastStatusTime = std::chrono::steady_clock::now();
    uint32_t lastXrunCount = 0;

    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        // Check for xruns
        uint32_t currentXruns = HoopiPi_GetXrunCount(backend);
        if (currentXruns != lastXrunCount) {
            std::cerr << "Xrun detected! Total: " << currentXruns << std::endl;
            lastXrunCount = currentXruns;
        }

        // Print status every 10 seconds
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastStatusTime);
        if (elapsed.count() >= 10) {
            printStatus(engine, backend);
            lastStatusTime = now;
        }
    }

    // Cleanup
    std::cout << "\nStopping audio..." << std::endl;
    HoopiPi_StopAudio(backend);

    std::cout << "Cleaning up..." << std::endl;
    HoopiPi_DeleteAlsaBackend(backend);
    HoopiPi_Cleanup(engine);
    HoopiPi_DeleteEngine(engine);

    std::cout << "Shutdown complete" << std::endl;

    return 0;
}
