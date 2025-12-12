#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
#define HOOPIPI_EXTERN extern __declspec(dllexport)
#else
#define HOOPIPI_EXTERN extern
#endif

// Opaque handle types
typedef struct HoopiPi_Engine HoopiPi_Engine;
typedef struct HoopiPi_Backend HoopiPi_Backend;

// Callbacks
typedef void (*HoopiPi_ModelLoadCallback)(int slot, int success, const char* error, void* userData);

// ===== Engine Lifecycle =====

/**
 * @brief Create engine instance
 * @param sampleRate Sample rate in Hz (typically 48000)
 * @param bufferSize Maximum buffer size in samples (typically 128)
 * @return Engine handle or NULL on failure
 */
HOOPIPI_EXTERN HoopiPi_Engine* HoopiPi_CreateEngine(int sampleRate, int bufferSize);

/**
 * @brief Delete engine instance
 * @param engine Engine handle
 */
HOOPIPI_EXTERN void HoopiPi_DeleteEngine(HoopiPi_Engine* engine);

/**
 * @brief Initialize engine
 * @param engine Engine handle
 * @return 1 if successful, 0 on failure
 */
HOOPIPI_EXTERN int HoopiPi_Init(HoopiPi_Engine* engine);

/**
 * @brief Cleanup engine
 * @param engine Engine handle
 */
HOOPIPI_EXTERN void HoopiPi_Cleanup(HoopiPi_Engine* engine);

// ===== ALSA Backend =====

/**
 * @brief Create ALSA backend
 * @param engine Engine handle
 * @return Backend handle or NULL on failure
 */
HOOPIPI_EXTERN HoopiPi_Backend* HoopiPi_CreateAlsaBackend(HoopiPi_Engine* engine);

/**
 * @brief Delete ALSA backend
 * @param backend Backend handle
 */
HOOPIPI_EXTERN void HoopiPi_DeleteAlsaBackend(HoopiPi_Backend* backend);

/**
 * @brief Initialize ALSA backend
 * @param backend Backend handle
 * @param deviceName ALSA device name (e.g., "hw:DaisySeed")
 * @param sampleRate Sample rate in Hz
 * @param periodSize Period size in frames
 * @param numPeriods Number of periods (typically 2)
 * @return 1 if successful, 0 on failure
 */
HOOPIPI_EXTERN int HoopiPi_InitAlsa(HoopiPi_Backend* backend, const char* deviceName,
                                     int sampleRate, int periodSize, int numPeriods);

/**
 * @brief Start audio processing
 * @param backend Backend handle
 * @return 1 if successful, 0 on failure
 */
HOOPIPI_EXTERN int HoopiPi_StartAudio(HoopiPi_Backend* backend);

/**
 * @brief Stop audio processing
 * @param backend Backend handle
 */
HOOPIPI_EXTERN void HoopiPi_StopAudio(HoopiPi_Backend* backend);

/**
 * @brief Check if audio is running
 * @param backend Backend handle
 * @return 1 if running, 0 if stopped
 */
HOOPIPI_EXTERN int HoopiPi_IsAudioRunning(HoopiPi_Backend* backend);

// ===== Model Management =====

/**
 * @brief Load model asynchronously
 * @param engine Engine handle
 * @param slot Model slot (0 or 1)
 * @param modelPath Path to .nam or .json model file
 */
HOOPIPI_EXTERN void HoopiPi_LoadModelAsync(HoopiPi_Engine* engine, int slot, const char* modelPath);

/**
 * @brief Check if model is ready
 * @param engine Engine handle
 * @param slot Model slot (0 or 1)
 * @return 1 if ready, 0 if not
 */
HOOPIPI_EXTERN int HoopiPi_IsModelReady(HoopiPi_Engine* engine, int slot);

/**
 * @brief Set active model slot
 * @param engine Engine handle
 * @param slot Model slot (0 or 1)
 */
HOOPIPI_EXTERN void HoopiPi_SetActiveModel(HoopiPi_Engine* engine, int slot);

/**
 * @brief Get active model slot
 * @param engine Engine handle
 * @return Active slot number (0 or 1)
 */
HOOPIPI_EXTERN int HoopiPi_GetActiveModel(HoopiPi_Engine* engine);

/**
 * @brief Unload model from slot
 * @param engine Engine handle
 * @param slot Model slot (0 or 1)
 */
HOOPIPI_EXTERN void HoopiPi_UnloadModel(HoopiPi_Engine* engine, int slot);

// ===== Parameters =====

/**
 * @brief Set input gain
 * @param engine Engine handle
 * @param gainDB Gain in dB
 */
HOOPIPI_EXTERN void HoopiPi_SetInputGain(HoopiPi_Engine* engine, float gainDB);

/**
 * @brief Set output gain
 * @param engine Engine handle
 * @param gainDB Gain in dB
 */
HOOPIPI_EXTERN void HoopiPi_SetOutputGain(HoopiPi_Engine* engine, float gainDB);

/**
 * @brief Set bypass
 * @param engine Engine handle
 * @param bypass 1 to bypass, 0 to process
 */
HOOPIPI_EXTERN void HoopiPi_SetBypass(HoopiPi_Engine* engine, int bypass);

/**
 * @brief Set normalization
 * @param engine Engine handle
 * @param normalize 1 to enable, 0 to disable
 */
HOOPIPI_EXTERN void HoopiPi_SetNormalize(HoopiPi_Engine* engine, int normalize);

/**
 * @brief Set noise gate
 * @param engine Engine handle
 * @param enabled 1 to enable, 0 to disable
 * @param thresholdDB Threshold in dB
 */
HOOPIPI_EXTERN void HoopiPi_SetNoiseGate(HoopiPi_Engine* engine, int enabled, float thresholdDB);

/**
 * @brief Set DC blocker
 * @param engine Engine handle
 * @param enabled 1 to enable, 0 to disable
 */
HOOPIPI_EXTERN void HoopiPi_SetDCBlocker(HoopiPi_Engine* engine, int enabled);

// ===== Parameter Getters =====

HOOPIPI_EXTERN float HoopiPi_GetInputGain(HoopiPi_Engine* engine);
HOOPIPI_EXTERN float HoopiPi_GetOutputGain(HoopiPi_Engine* engine);
HOOPIPI_EXTERN int HoopiPi_GetBypass(HoopiPi_Engine* engine);
HOOPIPI_EXTERN int HoopiPi_GetNormalize(HoopiPi_Engine* engine);
HOOPIPI_EXTERN int HoopiPi_GetNoiseGateEnabled(HoopiPi_Engine* engine);
HOOPIPI_EXTERN float HoopiPi_GetNoiseGateThreshold(HoopiPi_Engine* engine);
HOOPIPI_EXTERN int HoopiPi_GetDCBlockerEnabled(HoopiPi_Engine* engine);

// ===== Monitoring =====

/**
 * @brief Get CPU load
 * @param engine Engine handle
 * @return CPU load as percentage (0.0 to 100.0)
 */
HOOPIPI_EXTERN float HoopiPi_GetCPULoad(HoopiPi_Engine* engine);

/**
 * @brief Get xrun count
 * @param backend Backend handle
 * @return Number of xruns since start
 */
HOOPIPI_EXTERN uint32_t HoopiPi_GetXrunCount(HoopiPi_Backend* backend);

/**
 * @brief Reset xrun counter
 * @param backend Backend handle
 */
HOOPIPI_EXTERN void HoopiPi_ResetXrunCount(HoopiPi_Backend* backend);

/**
 * @brief Get latency
 * @param backend Backend handle
 * @return Latency in milliseconds
 */
HOOPIPI_EXTERN float HoopiPi_GetLatency(HoopiPi_Backend* backend);

/**
 * @brief Get sample rate
 * @param backend Backend handle
 * @return Sample rate in Hz
 */
HOOPIPI_EXTERN int HoopiPi_GetSampleRate(HoopiPi_Backend* backend);

/**
 * @brief Get period size
 * @param backend Backend handle
 * @return Period size in frames
 */
HOOPIPI_EXTERN int HoopiPi_GetPeriodSize(HoopiPi_Backend* backend);

// ===== Callbacks =====

/**
 * @brief Set model load callback
 * @param engine Engine handle
 * @param callback Callback function
 * @param userData User data passed to callback
 */
HOOPIPI_EXTERN void HoopiPi_SetModelLoadCallback(HoopiPi_Engine* engine,
                                                   HoopiPi_ModelLoadCallback callback,
                                                   void* userData);

#ifdef __cplusplus
}
#endif
