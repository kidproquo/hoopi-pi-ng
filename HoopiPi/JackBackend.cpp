#include "JackBackend.h"
#include "Engine.h"
#include "../standalone/BackingTrack.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <unistd.h>

namespace HoopiPi {

JackBackend::JackBackend(Engine* engine)
    : m_engine(engine)
{
}

JackBackend::~JackBackend() {
    stop();

    // Only close if JACK didn't already close it via shutdown callback
    if (m_client && !m_shutdownCalled.load(std::memory_order_acquire)) {
        jack_client_close(m_client);
    }
    m_client = nullptr;
}

bool JackBackend::init(const std::string& clientName, bool autoConnect) {
    m_autoConnect = autoConnect;
    m_clientName = clientName;
    m_status.store(JackStatus::Connecting, std::memory_order_release);

    // Open JACK client
    jack_status_t status;
    m_client = jack_client_open(clientName.c_str(), JackNullOption, &status);

    if (!m_client) {
        std::string errorMsg = "Failed to open JACK client";
        if (status & JackServerFailed) {
            errorMsg += ": JACK server not running";
        }

        {
            std::lock_guard<std::mutex> lock(m_errorMutex);
            m_errorMessage = errorMsg;
        }
        m_status.store(JackStatus::Error, std::memory_order_release);

        std::cerr << errorMsg << std::endl;
        return false;
    }

    std::cout << "JACK client '" << clientName << "' opened" << std::endl;
    std::cout << "JACK sample rate: " << jack_get_sample_rate(m_client) << " Hz" << std::endl;
    std::cout << "JACK buffer size: " << jack_get_buffer_size(m_client) << " frames" << std::endl;

    // Set callbacks
    jack_set_process_callback(m_client, processCallback, this);
    jack_set_buffer_size_callback(m_client, bufferSizeCallback, this);
    jack_set_sample_rate_callback(m_client, sampleRateCallback, this);
    jack_set_xrun_callback(m_client, xrunCallback, this);
    jack_on_shutdown(m_client, shutdownCallback, this);

    // Create ports
    m_inputPortL = jack_port_register(m_client, "input_L",
                                       JACK_DEFAULT_AUDIO_TYPE,
                                       JackPortIsInput, 0);
    if (!m_inputPortL) {
        std::lock_guard<std::mutex> lock(m_errorMutex);
        m_errorMessage = "Failed to register input_L port";
        m_status.store(JackStatus::Error, std::memory_order_release);
        std::cerr << m_errorMessage << std::endl;
        return false;
    }

    m_inputPortR = jack_port_register(m_client, "input_R",
                                       JACK_DEFAULT_AUDIO_TYPE,
                                       JackPortIsInput, 0);
    if (!m_inputPortR) {
        std::lock_guard<std::mutex> lock(m_errorMutex);
        m_errorMessage = "Failed to register input_R port";
        m_status.store(JackStatus::Error, std::memory_order_release);
        std::cerr << m_errorMessage << std::endl;
        return false;
    }

    m_outputPortL = jack_port_register(m_client, "output_L",
                                        JACK_DEFAULT_AUDIO_TYPE,
                                        JackPortIsOutput, 0);
    if (!m_outputPortL) {
        std::lock_guard<std::mutex> lock(m_errorMutex);
        m_errorMessage = "Failed to register output_L port";
        m_status.store(JackStatus::Error, std::memory_order_release);
        std::cerr << m_errorMessage << std::endl;
        return false;
    }

    m_outputPortR = jack_port_register(m_client, "output_R",
                                        JACK_DEFAULT_AUDIO_TYPE,
                                        JackPortIsOutput, 0);
    if (!m_outputPortR) {
        std::lock_guard<std::mutex> lock(m_errorMutex);
        m_errorMessage = "Failed to register output_R port";
        m_status.store(JackStatus::Error, std::memory_order_release);
        std::cerr << m_errorMessage << std::endl;
        return false;
    }

    std::cout << "JACK ports registered" << std::endl;

    // Clear any previous error and mark as connected
    {
        std::lock_guard<std::mutex> lock(m_errorMutex);
        m_errorMessage.clear();
    }
    m_status.store(JackStatus::Connected, std::memory_order_release);

    return true;
}

bool JackBackend::start() {
    if (!m_client) {
        std::cerr << "JACK client not initialized" << std::endl;
        return false;
    }

    if (m_running.load(std::memory_order_acquire)) {
        return true; // Already running
    }

    // Activate client
    if (jack_activate(m_client) != 0) {
        std::cerr << "Failed to activate JACK client" << std::endl;
        return false;
    }

    m_running.store(true, std::memory_order_release);
    std::cout << "JACK client activated" << std::endl;

    // Auto-connect ports if requested
    if (m_autoConnect) {
        const char** ports;

        // Connect inputs to first available capture ports
        ports = jack_get_ports(m_client, nullptr, nullptr,
                              JackPortIsPhysical | JackPortIsOutput);
        if (ports) {
            if (ports[0]) {
                if (jack_connect(m_client, ports[0],
                               jack_port_name(m_inputPortL)) == 0) {
                    std::cout << "Connected input_L to " << ports[0] << std::endl;
                }
            }
            if (ports[1]) {
                if (jack_connect(m_client, ports[1],
                               jack_port_name(m_inputPortR)) == 0) {
                    std::cout << "Connected input_R to " << ports[1] << std::endl;
                }
            } else if (ports[0]) {
                // Mono input - connect both to same port
                if (jack_connect(m_client, ports[0],
                               jack_port_name(m_inputPortR)) == 0) {
                    std::cout << "Connected input_R to " << ports[0] << " (mono)" << std::endl;
                }
            }
        }
        jack_free(ports);

        // Connect outputs to first available playback ports
        ports = jack_get_ports(m_client, nullptr, nullptr,
                              JackPortIsPhysical | JackPortIsInput);
        if (ports) {
            if (ports[0]) {
                if (jack_connect(m_client, jack_port_name(m_outputPortL),
                               ports[0]) == 0) {
                    std::cout << "Connected output_L to " << ports[0] << std::endl;
                }
            }
            if (ports[1]) {
                if (jack_connect(m_client, jack_port_name(m_outputPortR),
                               ports[1]) == 0) {
                    std::cout << "Connected output_R to " << ports[1] << std::endl;
                }
            } else if (ports[0]) {
                // Mono output - connect both to same port
                if (jack_connect(m_client, jack_port_name(m_outputPortR),
                               ports[0]) == 0) {
                    std::cout << "Connected output_R to " << ports[0] << " (mono)" << std::endl;
                }
            }
        }
        jack_free(ports);
    }

    return true;
}

void JackBackend::stop() {
    if (!m_client || !m_running.load(std::memory_order_acquire)) {
        return;
    }

    jack_deactivate(m_client);
    m_running.store(false, std::memory_order_release);
    m_status.store(JackStatus::Disconnected, std::memory_order_release);

    std::cout << "JACK client deactivated" << std::endl;
}

std::string JackBackend::getErrorMessage() const {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    return m_errorMessage;
}

bool JackBackend::reconnect(const std::string& clientName, bool autoConnect) {
    std::cout << "Attempting to reconnect to JACK..." << std::endl;

    // Clean up existing connection
    stop();

    // Only close client if shutdown was NOT called by JACK
    // (JACK's shutdown callback means the client is already closed)
    bool shutdownByJack = m_shutdownCalled.load(std::memory_order_acquire);

    if (m_client && !shutdownByJack) {
        jack_client_close(m_client);
    }

    // Reset state
    m_client = nullptr;
    m_inputPortL = nullptr;
    m_inputPortR = nullptr;
    m_outputPortL = nullptr;
    m_outputPortR = nullptr;
    m_shutdownCalled.store(false, std::memory_order_release);

    // Try to initialize again
    if (!init(clientName, autoConnect)) {
        return false;
    }

    // Try to start
    if (!start()) {
        return false;
    }

    std::cout << "Successfully reconnected to JACK" << std::endl;
    return true;
}

int JackBackend::getSampleRate() const {
    if (!m_client) return 0;
    return jack_get_sample_rate(m_client);
}

int JackBackend::getBufferSize() const {
    if (!m_client) return 0;
    return jack_get_buffer_size(m_client);
}

float JackBackend::getLatencyMs() const {
    if (!m_client) return 0.0f;
    int sr = getSampleRate();
    if (sr == 0) return 0.0f;
    return (static_cast<float>(getBufferSize()) / sr) * 1000.0f;
}

float JackBackend::getCPULoad() const {
    if (!m_client) return 0.0f;
    return jack_cpu_load(m_client);
}

float JackBackend::getProcessCPUUsage() const {
    // Read process CPU usage from /proc/self/stat
    std::ifstream statFile("/proc/self/stat");
    if (!statFile.is_open()) {
        return -1.0f;
    }

    // Skip to the CPU time fields (13th and 14th fields after PID and comm)
    std::string dummy;
    unsigned long long utime, stime;

    // Format: pid (comm) state ... utime stime ...
    statFile >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy
             >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy
             >> utime >> stime;

    if (!statFile) {
        return -1.0f;
    }
    statFile.close();

    unsigned long long processTime = utime + stime;

    // Read system CPU time
    std::ifstream cpuFile("/proc/stat");
    if (!cpuFile.is_open()) {
        return -1.0f;
    }

    std::string cpu;
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
    cpuFile >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
    cpuFile.close();

    if (cpu != "cpu") {
        return -1.0f;
    }

    unsigned long long totalTime = user + nice + system + idle + iowait + irq + softirq + steal;

    // Calculate CPU usage percentage since last call
    if (m_lastTotalTime == 0) {
        // First call - initialize
        m_lastTotalTime = totalTime;
        m_lastProcessTime = processTime;
        return 0.0f;
    }

    unsigned long long totalDelta = totalTime - m_lastTotalTime;
    unsigned long long processDelta = processTime - m_lastProcessTime;

    m_lastTotalTime = totalTime;
    m_lastProcessTime = processTime;

    if (totalDelta == 0) {
        return 0.0f;
    }

    // CPU usage as percentage
    // Note: /proc/stat shows total CPU time across all cores, so we need to
    // multiply by number of cores to match what 'top' shows (where 100% = 1 full core)
    long numCores = sysconf(_SC_NPROCESSORS_ONLN);
    if (numCores <= 0) numCores = 1;

    float cpuPercent = (100.0f * processDelta * numCores) / totalDelta;

    return cpuPercent;
}

float JackBackend::getCPUTemperature() const {
    // Read CPU temperature from Raspberry Pi thermal zone
    std::ifstream tempFile("/sys/class/thermal/thermal_zone0/temp");
    if (!tempFile.is_open()) {
        return -1.0f; // Temperature unavailable
    }

    int milliDegrees = 0;
    tempFile >> milliDegrees;
    tempFile.close();

    // Convert from millidegrees to degrees Celsius
    return milliDegrees / 1000.0f;
}

float JackBackend::getMemoryUsage() const {
    // Read memory usage from /proc/self/status
    std::ifstream statusFile("/proc/self/status");
    if (!statusFile.is_open()) {
        return -1.0f; // Memory info unavailable
    }

    std::string line;
    while (std::getline(statusFile, line)) {
        // Look for VmRSS (Resident Set Size - actual RAM usage)
        if (line.substr(0, 6) == "VmRSS:") {
            // Extract the number from "VmRSS:    123456 kB"
            size_t colonPos = line.find(':');
            if (colonPos != std::string::npos) {
                std::string valueStr = line.substr(colonPos + 1);
                // Remove leading whitespace
                size_t firstDigit = valueStr.find_first_of("0123456789");
                if (firstDigit != std::string::npos) {
                    long memoryKB = std::stol(valueStr.substr(firstDigit));
                    statusFile.close();
                    // Convert KB to MB
                    return memoryKB / 1024.0f;
                }
            }
        }
    }

    statusFile.close();
    return -1.0f;
}

void JackBackend::setBackingTrack(BackingTrack* backingTrack) {
    m_backingTrack = backingTrack;
}

// ===== JACK Callbacks =====

int JackBackend::processCallback(jack_nframes_t nframes, void* arg) {
    JackBackend* backend = static_cast<JackBackend*>(arg);
    return backend->process(nframes);
}

int JackBackend::process(jack_nframes_t nframes) {
    // Get port buffers
    jack_default_audio_sample_t* inputL =
        static_cast<jack_default_audio_sample_t*>(
            jack_port_get_buffer(m_inputPortL, nframes));

    jack_default_audio_sample_t* inputR =
        static_cast<jack_default_audio_sample_t*>(
            jack_port_get_buffer(m_inputPortR, nframes));

    jack_default_audio_sample_t* outputL =
        static_cast<jack_default_audio_sample_t*>(
            jack_port_get_buffer(m_outputPortL, nframes));

    jack_default_audio_sample_t* outputR =
        static_cast<jack_default_audio_sample_t*>(
            jack_port_get_buffer(m_outputPortR, nframes));

    // Process through engine
    if (m_engine) {
        // JACK uses float by default, so no conversion needed!
        m_engine->processStereo(inputL, inputR, outputL, outputR, nframes);
    } else {
        // No engine - pass through
        std::memcpy(outputL, inputL, nframes * sizeof(jack_default_audio_sample_t));
        std::memcpy(outputR, inputR, nframes * sizeof(jack_default_audio_sample_t));
    }

    // Mix backing track if playing
    if (m_backingTrack && m_backingTrack->isPlaying()) {
        // Allocate temporary buffers on stack (RT-safe)
        float trackBufferL[nframes];
        float trackBufferR[nframes];

        // Fill backing track buffers
        m_backingTrack->fillBuffer(trackBufferL, trackBufferR, nframes);

        // Mix with guitar output
        for (size_t i = 0; i < nframes; i++) {
            outputL[i] += trackBufferL[i];
            outputR[i] += trackBufferR[i];
        }
    }

    return 0;
}

int JackBackend::bufferSizeCallback(jack_nframes_t nframes, void* arg) {
    JackBackend* backend = static_cast<JackBackend*>(arg);
    std::cout << "JACK buffer size changed to " << nframes << " frames" << std::endl;
    return 0;
}

int JackBackend::sampleRateCallback(jack_nframes_t nframes, void* arg) {
    JackBackend* backend = static_cast<JackBackend*>(arg);
    std::cout << "JACK sample rate changed to " << nframes << " Hz" << std::endl;
    return 0;
}

int JackBackend::xrunCallback(void* arg) {
    JackBackend* backend = static_cast<JackBackend*>(arg);
    backend->m_xrunCount.fetch_add(1, std::memory_order_relaxed);
    std::cerr << "JACK xrun occurred (count: "
              << backend->m_xrunCount.load() << ")" << std::endl;
    return 0;
}

void JackBackend::shutdownCallback(void* arg) {
    JackBackend* backend = static_cast<JackBackend*>(arg);
    backend->m_running.store(false, std::memory_order_release);
    backend->m_status.store(JackStatus::Disconnected, std::memory_order_release);
    backend->m_shutdownCalled.store(true, std::memory_order_release);  // JACK closed the client
    {
        std::lock_guard<std::mutex> lock(backend->m_errorMutex);
        backend->m_errorMessage = "JACK server shut down";
    }
    std::cerr << "JACK server shut down - will attempt to reconnect" << std::endl;
}

} // namespace HoopiPi
