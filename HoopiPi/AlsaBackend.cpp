#include "AlsaBackend.h"
#include "Engine.h"
#include <iostream>
#include <cstring>
#include <sched.h>
#include <sys/mman.h>
#include <cmath>

namespace HoopiPi {

AlsaBackend::AlsaBackend(Engine* engine)
    : m_engine(engine)
{
}

AlsaBackend::~AlsaBackend() {
    stop();
    closePCM();

    // Free buffers
    delete[] m_inputBufferS16;
    delete[] m_outputBufferS16;
    delete[] m_inputBufferFloat;
    delete[] m_outputBufferFloat;
}

bool AlsaBackend::init(const std::string& deviceName, int sampleRate,
                       int periodSize, int numPeriods) {
    m_deviceName = deviceName;
    m_sampleRate = sampleRate;
    m_periodSize = periodSize;
    m_bufferSize = periodSize * numPeriods;

    // Allocate buffers (will be sized properly after channel detection)
    m_inputBufferS16 = nullptr;
    m_outputBufferS16 = nullptr;
    m_inputBufferFloat = nullptr;
    m_outputBufferFloat = nullptr;

    // Setup ALSA PCM
    if (!setupPCM(deviceName)) {
        std::cerr << "Failed to setup ALSA PCM" << std::endl;
        return false;
    }

    return true;
}

bool AlsaBackend::start() {
    if (m_running.load(std::memory_order_acquire)) {
        return true; // Already running
    }

    m_shouldStop.store(false, std::memory_order_release);
    m_running.store(true, std::memory_order_release);

    // Start audio thread
    m_audioThread = std::thread(&AlsaBackend::audioThread, this);

    return true;
}

void AlsaBackend::stop() {
    if (!m_running.load(std::memory_order_acquire)) {
        return; // Not running
    }

    m_shouldStop.store(true, std::memory_order_release);

    if (m_audioThread.joinable()) {
        m_audioThread.join();
    }

    m_running.store(false, std::memory_order_release);
}

float AlsaBackend::getLatencyMs() const {
    return (static_cast<float>(m_bufferSize) / m_sampleRate) * 1000.0f;
}

// ===== ALSA Setup =====

bool AlsaBackend::setupPCM(const std::string& deviceName) {
    int err;

    // Open PCM device for capture
    err = snd_pcm_open(&m_captureHandle, deviceName.c_str(),
                       SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        std::cerr << "Cannot open capture device " << deviceName << ": "
                  << snd_strerror(err) << std::endl;
        return false;
    }

    // Open PCM device for playback
    err = snd_pcm_open(&m_playbackHandle, deviceName.c_str(),
                       SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        std::cerr << "Cannot open playback device " << deviceName << ": "
                  << snd_strerror(err) << std::endl;
        snd_pcm_close(m_captureHandle);
        m_captureHandle = nullptr;
        return false;
    }

    // Set hardware parameters
    if (!setHWParams()) {
        closePCM();
        return false;
    }

    // Set software parameters
    if (!setSWParams()) {
        closePCM();
        return false;
    }

    // Link capture and playback streams for synchronization
    err = snd_pcm_link(m_captureHandle, m_playbackHandle);
    if (err < 0) {
        std::cerr << "Warning: Cannot link streams: " << snd_strerror(err) << std::endl;
        // Not fatal, continue anyway
    } else {
        std::cout << "Capture and playback streams linked" << std::endl;
    }

    return true;
}

bool AlsaBackend::setHWParams() {
    int err;
    snd_pcm_hw_params_t* hw_params;

    // === Capture HW params ===
    snd_pcm_hw_params_alloca(&hw_params);

    err = snd_pcm_hw_params_any(m_captureHandle, hw_params);
    if (err < 0) {
        std::cerr << "Cannot initialize capture hw params: " << snd_strerror(err) << std::endl;
        return false;
    }

    err = snd_pcm_hw_params_set_access(m_captureHandle, hw_params,
                                       SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        std::cerr << "Cannot set capture access type: " << snd_strerror(err) << std::endl;
        return false;
    }

    err = snd_pcm_hw_params_set_format(m_captureHandle, hw_params,
                                       SND_PCM_FORMAT_S16_LE);
    if (err < 0) {
        std::cerr << "Cannot set capture sample format: " << snd_strerror(err) << std::endl;
        return false;
    }

    unsigned int rate = m_sampleRate;
    err = snd_pcm_hw_params_set_rate_near(m_captureHandle, hw_params, &rate, 0);
    if (err < 0) {
        std::cerr << "Cannot set capture sample rate: " << snd_strerror(err) << std::endl;
        return false;
    }
    m_sampleRate = rate;

    // Try stereo first, fall back to mono
    err = snd_pcm_hw_params_set_channels(m_captureHandle, hw_params, 2); // Stereo
    if (err < 0) {
        // Try mono
        err = snd_pcm_hw_params_set_channels(m_captureHandle, hw_params, 1);
        if (err < 0) {
            std::cerr << "Cannot set capture channel count: " << snd_strerror(err) << std::endl;
            return false;
        }
        m_numChannels = 1;
        std::cout << "Using mono input" << std::endl;
    } else {
        m_numChannels = 2;
        std::cout << "Using stereo input (will process left channel only)" << std::endl;
    }

    snd_pcm_uframes_t periodSize = m_periodSize;
    err = snd_pcm_hw_params_set_period_size_near(m_captureHandle, hw_params,
                                                  &periodSize, 0);
    if (err < 0) {
        std::cerr << "Cannot set capture period size: " << snd_strerror(err) << std::endl;
        return false;
    }
    m_periodSize = periodSize;

    snd_pcm_uframes_t bufferSize = m_bufferSize;
    err = snd_pcm_hw_params_set_buffer_size_near(m_captureHandle, hw_params,
                                                  &bufferSize);
    if (err < 0) {
        std::cerr << "Cannot set capture buffer size: " << snd_strerror(err) << std::endl;
        return false;
    }
    m_bufferSize = bufferSize;

    err = snd_pcm_hw_params(m_captureHandle, hw_params);
    if (err < 0) {
        std::cerr << "Cannot set capture hw params: " << snd_strerror(err) << std::endl;
        return false;
    }

    // === Playback HW params ===
    snd_pcm_hw_params_alloca(&hw_params);

    err = snd_pcm_hw_params_any(m_playbackHandle, hw_params);
    if (err < 0) {
        std::cerr << "Cannot initialize playback hw params: " << snd_strerror(err) << std::endl;
        return false;
    }

    err = snd_pcm_hw_params_set_access(m_playbackHandle, hw_params,
                                       SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        std::cerr << "Cannot set playback access type: " << snd_strerror(err) << std::endl;
        return false;
    }

    err = snd_pcm_hw_params_set_format(m_playbackHandle, hw_params,
                                       SND_PCM_FORMAT_S16_LE);
    if (err < 0) {
        std::cerr << "Cannot set playback sample format: " << snd_strerror(err) << std::endl;
        return false;
    }

    rate = m_sampleRate;
    err = snd_pcm_hw_params_set_rate_near(m_playbackHandle, hw_params, &rate, 0);
    if (err < 0) {
        std::cerr << "Cannot set playback sample rate: " << snd_strerror(err) << std::endl;
        return false;
    }

    // Try stereo first, fall back to mono
    err = snd_pcm_hw_params_set_channels(m_playbackHandle, hw_params, 2); // Stereo
    if (err < 0) {
        // Try mono
        err = snd_pcm_hw_params_set_channels(m_playbackHandle, hw_params, 1);
        if (err < 0) {
            std::cerr << "Cannot set playback channel count: " << snd_strerror(err) << std::endl;
            return false;
        }
        std::cout << "Using mono output" << std::endl;
    } else {
        std::cout << "Using stereo output (duplicating processed signal)" << std::endl;
    }

    periodSize = m_periodSize;
    err = snd_pcm_hw_params_set_period_size_near(m_playbackHandle, hw_params,
                                                  &periodSize, 0);
    if (err < 0) {
        std::cerr << "Cannot set playback period size: " << snd_strerror(err) << std::endl;
        return false;
    }

    bufferSize = m_bufferSize;
    err = snd_pcm_hw_params_set_buffer_size_near(m_playbackHandle, hw_params,
                                                  &bufferSize);
    if (err < 0) {
        std::cerr << "Cannot set playback buffer size: " << snd_strerror(err) << std::endl;
        return false;
    }

    err = snd_pcm_hw_params(m_playbackHandle, hw_params);
    if (err < 0) {
        std::cerr << "Cannot set playback hw params: " << snd_strerror(err) << std::endl;
        return false;
    }

    std::cout << "ALSA configured: " << m_sampleRate << " Hz, "
              << m_periodSize << " frames/period, "
              << m_bufferSize << " frames buffer, "
              << m_numChannels << " channels" << std::endl;

    // Now allocate buffers with correct channel count
    m_inputBufferS16 = new int16_t[m_periodSize * m_numChannels];
    m_outputBufferS16 = new int16_t[m_periodSize * m_numChannels];
    m_inputBufferFloat = new float[m_periodSize];
    m_outputBufferFloat = new float[m_periodSize];

    return true;
}

bool AlsaBackend::setSWParams() {
    int err;
    snd_pcm_sw_params_t* sw_params;

    // Configure capture software parameters
    snd_pcm_sw_params_alloca(&sw_params);

    err = snd_pcm_sw_params_current(m_captureHandle, sw_params);
    if (err < 0) {
        std::cerr << "Cannot get capture sw params: " << snd_strerror(err) << std::endl;
        return false;
    }

    // Start capture immediately
    err = snd_pcm_sw_params_set_start_threshold(m_captureHandle, sw_params, 0);
    if (err < 0) {
        std::cerr << "Cannot set capture start threshold: " << snd_strerror(err) << std::endl;
        return false;
    }

    err = snd_pcm_sw_params(m_captureHandle, sw_params);
    if (err < 0) {
        std::cerr << "Cannot set capture sw params: " << snd_strerror(err) << std::endl;
        return false;
    }

    // Configure playback software parameters
    snd_pcm_sw_params_alloca(&sw_params);

    err = snd_pcm_sw_params_current(m_playbackHandle, sw_params);
    if (err < 0) {
        std::cerr << "Cannot get playback sw params: " << snd_strerror(err) << std::endl;
        return false;
    }

    // Start playback when buffer is half full
    err = snd_pcm_sw_params_set_start_threshold(m_playbackHandle, sw_params, m_bufferSize / 2);
    if (err < 0) {
        std::cerr << "Cannot set playback start threshold: " << snd_strerror(err) << std::endl;
        return false;
    }

    err = snd_pcm_sw_params(m_playbackHandle, sw_params);
    if (err < 0) {
        std::cerr << "Cannot set playback sw params: " << snd_strerror(err) << std::endl;
        return false;
    }

    return true;
}

void AlsaBackend::closePCM() {
    if (m_captureHandle) {
        snd_pcm_close(m_captureHandle);
        m_captureHandle = nullptr;
    }

    if (m_playbackHandle) {
        snd_pcm_close(m_playbackHandle);
        m_playbackHandle = nullptr;
    }
}

// ===== Audio Thread =====

void AlsaBackend::audioThread() {
    // Set real-time priority
    struct sched_param param;
    param.sched_priority = RT_PRIORITY;
    if (sched_setscheduler(0, SCHED_FIFO, &param) != 0) {
        std::cerr << "Warning: Could not set real-time priority" << std::endl;
    }

    // Lock memory to prevent page faults
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
        std::cerr << "Warning: Could not lock memory" << std::endl;
    }

    // Prepare devices
    snd_pcm_prepare(m_captureHandle);
    snd_pcm_prepare(m_playbackHandle);

    // Prime the playback buffer with silence to prevent initial underrun
    int16_t* silenceBuffer = new int16_t[m_periodSize * m_numChannels];
    std::memset(silenceBuffer, 0, m_periodSize * m_numChannels * sizeof(int16_t));

    // Write one period of silence to get playback started
    snd_pcm_writei(m_playbackHandle, silenceBuffer, m_periodSize);

    delete[] silenceBuffer;

    std::cout << "Audio thread started" << std::endl;

    // Main audio loop
    while (!m_shouldStop.load(std::memory_order_acquire)) {
        int result = audioCallback();
        if (result < 0) {
            // Error occurred, try to recover
            if (xrunRecovery(result) < 0) {
                std::cerr << "Fatal audio error, stopping" << std::endl;
                break;
            }
        }
    }

    // Drain and stop
    snd_pcm_drop(m_captureHandle);
    snd_pcm_drop(m_playbackHandle);

    std::cout << "Audio thread stopped" << std::endl;
}

int AlsaBackend::audioCallback() {
    // Read input
    snd_pcm_sframes_t framesRead = snd_pcm_readi(m_captureHandle, m_inputBufferS16,
                                                  m_periodSize);
    if (framesRead < 0) {
        return static_cast<int>(framesRead);
    }

    if (framesRead != m_periodSize) {
        std::cerr << "Short read: " << framesRead << " / " << m_periodSize << std::endl;
    }

    // Convert to float (deinterleave if stereo, take left channel)
    if (m_numChannels == 2) {
        // Deinterleave: take every other sample (left channel)
        for (snd_pcm_sframes_t i = 0; i < framesRead; ++i) {
            m_inputBufferFloat[i] = static_cast<float>(m_inputBufferS16[i * 2]) / 32768.0f;
        }
    } else {
        // Mono: direct conversion
        int16ToFloat(m_inputBufferS16, m_inputBufferFloat, framesRead);
    }

    // Process through engine
    if (m_engine) {
        m_engine->process(m_inputBufferFloat, m_outputBufferFloat, framesRead);
    } else {
        // No engine - copy input to output
        std::memcpy(m_outputBufferFloat, m_inputBufferFloat,
                    framesRead * sizeof(float));
    }

    // Convert to int16 and interleave if stereo
    if (m_numChannels == 2) {
        // Interleave: duplicate mono output to both channels
        for (snd_pcm_sframes_t i = 0; i < framesRead; ++i) {
            float sample = m_outputBufferFloat[i] * 32767.0f;
            if (sample > 32767.0f) sample = 32767.0f;
            if (sample < -32768.0f) sample = -32768.0f;
            int16_t s16 = static_cast<int16_t>(sample);
            m_outputBufferS16[i * 2] = s16;     // Left
            m_outputBufferS16[i * 2 + 1] = s16; // Right
        }
    } else {
        // Mono: direct conversion
        floatToInt16(m_outputBufferFloat, m_outputBufferS16, framesRead);
    }

    // Write output
    snd_pcm_sframes_t framesWritten = snd_pcm_writei(m_playbackHandle, m_outputBufferS16,
                                                      framesRead);
    if (framesWritten < 0) {
        return static_cast<int>(framesWritten);
    }

    if (framesWritten != framesRead) {
        std::cerr << "Short write: " << framesWritten << " / " << framesRead << std::endl;
    }

    return 0;
}

// ===== Format Conversion =====

void AlsaBackend::int16ToFloat(const int16_t* input, float* output, size_t numSamples) {
    constexpr float scale = 1.0f / 32768.0f;
    for (size_t i = 0; i < numSamples; ++i) {
        output[i] = static_cast<float>(input[i]) * scale;
    }
}

void AlsaBackend::floatToInt16(const float* input, int16_t* output, size_t numSamples) {
    constexpr float scale = 32767.0f;
    for (size_t i = 0; i < numSamples; ++i) {
        float sample = input[i] * scale;

        // Clamp
        if (sample > 32767.0f) sample = 32767.0f;
        if (sample < -32768.0f) sample = -32768.0f;

        output[i] = static_cast<int16_t>(sample);
    }
}

// ===== Error Recovery =====

int AlsaBackend::xrunRecovery(int err) {
    if (err == -EPIPE) {
        // Underrun/overrun
        m_xrunCount.fetch_add(1, std::memory_order_relaxed);

        std::cerr << "Xrun occurred (count: " << m_xrunCount.load() << ")" << std::endl;

        // For linked streams, only prepare the capture (master) stream
        // Playback will follow automatically
        err = snd_pcm_prepare(m_captureHandle);
        if (err < 0) {
            std::cerr << "Cannot recover from xrun: " << snd_strerror(err) << std::endl;
            return err;
        }

        // Re-prime playback buffer with silence
        int16_t* silenceBuffer = new int16_t[m_periodSize * m_numChannels];
        std::memset(silenceBuffer, 0, m_periodSize * m_numChannels * sizeof(int16_t));
        snd_pcm_writei(m_playbackHandle, silenceBuffer, m_periodSize);
        delete[] silenceBuffer;

        return 0;
    } else if (err == -ESTRPIPE) {
        // Suspended
        std::cerr << "Stream suspended" << std::endl;

        while ((err = snd_pcm_resume(m_captureHandle)) == -EAGAIN) {
            sleep(1);
        }

        if (err < 0) {
            err = snd_pcm_prepare(m_captureHandle);
            if (err < 0) {
                std::cerr << "Cannot recover from suspend: " << snd_strerror(err) << std::endl;
                return err;
            }
        }

        err = snd_pcm_prepare(m_playbackHandle);
        if (err < 0) {
            std::cerr << "Cannot recover playback from suspend: " << snd_strerror(err) << std::endl;
            return err;
        }

        return 0;
    }

    return err;
}

} // namespace HoopiPi
