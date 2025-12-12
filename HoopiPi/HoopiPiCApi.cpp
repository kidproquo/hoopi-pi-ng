#include "HoopiPiCApi.h"
#include "Engine.h"
#include "AlsaBackend.h"
#include <cstring>

// Opaque struct definitions
struct HoopiPi_Engine {
    HoopiPi::Engine* engine;
    HoopiPi_ModelLoadCallback callback;
    void* callbackUserData;
};

struct HoopiPi_Backend {
    HoopiPi::AlsaBackend* backend;
};

// ===== Engine Lifecycle =====

HoopiPi_Engine* HoopiPi_CreateEngine(int sampleRate, int bufferSize) {
    try {
        HoopiPi_Engine* handle = new HoopiPi_Engine();
        handle->engine = new HoopiPi::Engine(sampleRate, bufferSize);
        handle->callback = nullptr;
        handle->callbackUserData = nullptr;
        return handle;
    } catch (...) {
        return nullptr;
    }
}

void HoopiPi_DeleteEngine(HoopiPi_Engine* engine) {
    if (engine) {
        delete engine->engine;
        delete engine;
    }
}

int HoopiPi_Init(HoopiPi_Engine* engine) {
    if (!engine || !engine->engine) return 0;
    return engine->engine->init() ? 1 : 0;
}

void HoopiPi_Cleanup(HoopiPi_Engine* engine) {
    if (engine && engine->engine) {
        engine->engine->cleanup();
    }
}

// ===== ALSA Backend =====

HoopiPi_Backend* HoopiPi_CreateAlsaBackend(HoopiPi_Engine* engine) {
    if (!engine || !engine->engine) return nullptr;

    try {
        HoopiPi_Backend* handle = new HoopiPi_Backend();
        handle->backend = new HoopiPi::AlsaBackend(engine->engine);
        return handle;
    } catch (...) {
        return nullptr;
    }
}

void HoopiPi_DeleteAlsaBackend(HoopiPi_Backend* backend) {
    if (backend) {
        delete backend->backend;
        delete backend;
    }
}

int HoopiPi_InitAlsa(HoopiPi_Backend* backend, const char* deviceName,
                      int sampleRate, int periodSize, int numPeriods) {
    if (!backend || !backend->backend || !deviceName) return 0;

    return backend->backend->init(deviceName, sampleRate, periodSize, numPeriods) ? 1 : 0;
}

int HoopiPi_StartAudio(HoopiPi_Backend* backend) {
    if (!backend || !backend->backend) return 0;
    return backend->backend->start() ? 1 : 0;
}

void HoopiPi_StopAudio(HoopiPi_Backend* backend) {
    if (backend && backend->backend) {
        backend->backend->stop();
    }
}

int HoopiPi_IsAudioRunning(HoopiPi_Backend* backend) {
    if (!backend || !backend->backend) return 0;
    return backend->backend->isRunning() ? 1 : 0;
}

// ===== Model Management =====

void HoopiPi_LoadModelAsync(HoopiPi_Engine* engine, int slot, const char* modelPath) {
    if (engine && engine->engine && modelPath) {
        engine->engine->loadModelAsync(slot, modelPath);
    }
}

int HoopiPi_IsModelReady(HoopiPi_Engine* engine, int slot) {
    if (!engine || !engine->engine) return 0;
    return engine->engine->isModelReady(slot) ? 1 : 0;
}

void HoopiPi_SetActiveModel(HoopiPi_Engine* engine, int slot) {
    if (engine && engine->engine) {
        engine->engine->setActiveModel(slot);
    }
}

int HoopiPi_GetActiveModel(HoopiPi_Engine* engine) {
    if (!engine || !engine->engine) return 0;
    return engine->engine->getActiveModel();
}

void HoopiPi_UnloadModel(HoopiPi_Engine* engine, int slot) {
    if (engine && engine->engine) {
        engine->engine->unloadModel(slot);
    }
}

// ===== Parameters =====

void HoopiPi_SetInputGain(HoopiPi_Engine* engine, float gainDB) {
    if (engine && engine->engine) {
        engine->engine->setInputGain(gainDB);
    }
}

void HoopiPi_SetOutputGain(HoopiPi_Engine* engine, float gainDB) {
    if (engine && engine->engine) {
        engine->engine->setOutputGain(gainDB);
    }
}

void HoopiPi_SetBypass(HoopiPi_Engine* engine, int bypass) {
    if (engine && engine->engine) {
        engine->engine->setBypass(bypass != 0);
    }
}

void HoopiPi_SetNormalize(HoopiPi_Engine* engine, int normalize) {
    if (engine && engine->engine) {
        engine->engine->setNormalize(normalize != 0);
    }
}

void HoopiPi_SetNoiseGate(HoopiPi_Engine* engine, int enabled, float thresholdDB) {
    if (engine && engine->engine) {
        engine->engine->setNoiseGate(enabled != 0, thresholdDB);
    }
}

void HoopiPi_SetDCBlocker(HoopiPi_Engine* engine, int enabled) {
    if (engine && engine->engine) {
        engine->engine->setDCBlocker(enabled != 0);
    }
}

// ===== Parameter Getters =====

float HoopiPi_GetInputGain(HoopiPi_Engine* engine) {
    if (!engine || !engine->engine) return 0.0f;
    return engine->engine->getInputGain();
}

float HoopiPi_GetOutputGain(HoopiPi_Engine* engine) {
    if (!engine || !engine->engine) return 0.0f;
    return engine->engine->getOutputGain();
}

int HoopiPi_GetBypass(HoopiPi_Engine* engine) {
    if (!engine || !engine->engine) return 0;
    return engine->engine->getBypass() ? 1 : 0;
}

int HoopiPi_GetNormalize(HoopiPi_Engine* engine) {
    if (!engine || !engine->engine) return 0;
    return engine->engine->getNormalize() ? 1 : 0;
}

int HoopiPi_GetNoiseGateEnabled(HoopiPi_Engine* engine) {
    if (!engine || !engine->engine) return 0;
    return engine->engine->getNoiseGateEnabled() ? 1 : 0;
}

float HoopiPi_GetNoiseGateThreshold(HoopiPi_Engine* engine) {
    if (!engine || !engine->engine) return 0.0f;
    return engine->engine->getNoiseGateThreshold();
}

int HoopiPi_GetDCBlockerEnabled(HoopiPi_Engine* engine) {
    if (!engine || !engine->engine) return 0;
    return engine->engine->getDCBlockerEnabled() ? 1 : 0;
}

// ===== Monitoring =====

float HoopiPi_GetCPULoad(HoopiPi_Engine* engine) {
    if (!engine || !engine->engine) return 0.0f;
    // CPU load is only available from the backend (JackBackend::getCPULoad())
    // The C API doesn't have access to the backend, so return 0
    return 0.0f;
}

uint32_t HoopiPi_GetXrunCount(HoopiPi_Backend* backend) {
    if (!backend || !backend->backend) return 0;
    return backend->backend->getXrunCount();
}

void HoopiPi_ResetXrunCount(HoopiPi_Backend* backend) {
    if (backend && backend->backend) {
        backend->backend->resetXrunCount();
    }
}

float HoopiPi_GetLatency(HoopiPi_Backend* backend) {
    if (!backend || !backend->backend) return 0.0f;
    return backend->backend->getLatencyMs();
}

int HoopiPi_GetSampleRate(HoopiPi_Backend* backend) {
    if (!backend || !backend->backend) return 0;
    return backend->backend->getSampleRate();
}

int HoopiPi_GetPeriodSize(HoopiPi_Backend* backend) {
    if (!backend || !backend->backend) return 0;
    return backend->backend->getPeriodSize();
}

// ===== Callbacks =====

void HoopiPi_SetModelLoadCallback(HoopiPi_Engine* engine,
                                    HoopiPi_ModelLoadCallback callback,
                                    void* userData) {
    if (engine) {
        engine->callback = callback;
        engine->callbackUserData = userData;

        if (engine->engine && callback) {
            // Wrap C callback for C++ engine
            engine->engine->setModelLoadCallback(
                [engine](int slot, bool success, const std::string& error) {
                    if (engine->callback) {
                        engine->callback(slot, success ? 1 : 0, error.c_str(),
                                       engine->callbackUserData);
                    }
                }
            );
        }
    }
}
