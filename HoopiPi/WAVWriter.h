#pragma once

#include <fstream>
#include <string>
#include <cstring>
#include <cstdint>
#include <algorithm>

namespace HoopiPi {

/**
 * @brief Simple WAV file writer for stereo 16-bit PCM audio
 *
 * Writes standard WAV files with PCM format. Converts float samples
 * to 16-bit integers. Not real-time safe - use from background thread only.
 */
class WAVWriter {
public:
    WAVWriter() = default;
    ~WAVWriter() {
        close();
    }

    /**
     * @brief Open WAV file for writing
     * @param path File path
     * @param sampleRate Sample rate in Hz
     * @param numChannels Number of channels (1 = mono, 2 = stereo)
     * @return true if file opened successfully
     */
    bool open(const std::string& path, uint32_t sampleRate, uint16_t numChannels) {
        if (m_file.is_open()) {
            close();
        }

        m_file.open(path, std::ios::binary);
        if (!m_file.is_open()) {
            return false;
        }

        m_sampleRate = sampleRate;
        m_numChannels = numChannels;
        m_dataSize = 0;

        // Write placeholder header (will be updated in close())
        writeHeader();

        return true;
    }

    /**
     * @brief Write audio frames to WAV file
     * @param data Interleaved float samples [-1.0, 1.0]
     * @param numFrames Number of frames (samples per channel)
     */
    void write(const float* data, size_t numFrames) {
        if (!m_file.is_open()) {
            return;
        }

        const size_t totalSamples = numFrames * m_numChannels;

        for (size_t i = 0; i < totalSamples; ++i) {
            // Clamp to [-1.0, 1.0] and convert to int16
            float sample = std::max(-1.0f, std::min(1.0f, data[i]));
            int16_t pcm = static_cast<int16_t>(sample * 32767.0f);

            m_file.write(reinterpret_cast<const char*>(&pcm), sizeof(int16_t));
        }

        m_dataSize += totalSamples * sizeof(int16_t);
    }

    /**
     * @brief Close file and finalize WAV header
     */
    void close() {
        if (!m_file.is_open()) {
            return;
        }

        // Seek back to beginning and write correct header
        m_file.seekp(0);
        writeHeader();

        m_file.close();
        m_dataSize = 0;
    }

    /**
     * @brief Check if file is open
     */
    bool isOpen() const {
        return m_file.is_open();
    }

    /**
     * @brief Get current data size in bytes
     */
    uint32_t getDataSize() const {
        return m_dataSize;
    }

    /**
     * @brief Get duration in seconds
     */
    float getDuration() const {
        if (m_sampleRate == 0 || m_numChannels == 0) {
            return 0.0f;
        }
        const uint32_t numFrames = m_dataSize / (m_numChannels * sizeof(int16_t));
        return static_cast<float>(numFrames) / static_cast<float>(m_sampleRate);
    }

private:
    void writeHeader() {
        const uint16_t bitsPerSample = 16;
        const uint16_t blockAlign = m_numChannels * (bitsPerSample / 8);
        const uint32_t byteRate = m_sampleRate * blockAlign;
        const uint32_t chunkSize = 36 + m_dataSize;

        // RIFF header
        m_file.write("RIFF", 4);
        m_file.write(reinterpret_cast<const char*>(&chunkSize), 4);
        m_file.write("WAVE", 4);

        // fmt subchunk
        m_file.write("fmt ", 4);
        uint32_t fmtSize = 16;
        m_file.write(reinterpret_cast<const char*>(&fmtSize), 4);

        uint16_t audioFormat = 1; // PCM
        m_file.write(reinterpret_cast<const char*>(&audioFormat), 2);
        m_file.write(reinterpret_cast<const char*>(&m_numChannels), 2);
        m_file.write(reinterpret_cast<const char*>(&m_sampleRate), 4);
        m_file.write(reinterpret_cast<const char*>(&byteRate), 4);
        m_file.write(reinterpret_cast<const char*>(&blockAlign), 2);
        m_file.write(reinterpret_cast<const char*>(&bitsPerSample), 2);

        // data subchunk
        m_file.write("data", 4);
        m_file.write(reinterpret_cast<const char*>(&m_dataSize), 4);
    }

    std::ofstream m_file;
    uint32_t m_sampleRate{0};
    uint16_t m_numChannels{0};
    uint32_t m_dataSize{0};
};

} // namespace HoopiPi
