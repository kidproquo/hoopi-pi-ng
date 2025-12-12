#pragma once

#include "WAVWriter.h"
#include <atomic>
#include <thread>
#include <memory>
#include <string>
#include <chrono>
#include <iostream>
#include <filesystem>

namespace HoopiPi {

/**
 * @brief Lock-free audio recorder for real-time safe recording to WAV files
 *
 * Uses a lock-free SPSC (single producer, single consumer) ring buffer
 * to decouple real-time audio processing from disk I/O.
 *
 * RT thread calls pushAudioFrame() - lock-free, ~50-100ns overhead
 * Background thread writes buffered audio to WAV file
 */
class AudioRecorder {
public:
    /**
     * @brief Construct recorder
     * @param recordingsDir Directory to store recordings
     */
    AudioRecorder(const std::string& recordingsDir)
        : m_recordingsDir(recordingsDir)
    {
        // Create recordings directory if it doesn't exist
        std::filesystem::create_directories(recordingsDir);
    }

    ~AudioRecorder() {
        stopRecording();
    }

    /**
     * @brief Start recording to WAV file
     * @param filename Optional filename (auto-generated if empty)
     * @param sampleRate Sample rate in Hz
     * @return Full path to recording file, or empty string on error
     */
    std::string startRecording(const std::string& filename, int sampleRate) {
        if (m_recording.load(std::memory_order_acquire)) {
            std::cerr << "Already recording" << std::endl;
            return "";
        }

        // Generate filename if not provided
        std::string actualFilename = filename;
        if (actualFilename.empty()) {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            char buf[64];
            std::strftime(buf, sizeof(buf), "recording-%Y-%m-%d-%H%M%S.wav", std::localtime(&time_t));
            actualFilename = buf;
        } else {
            // Ensure .wav extension
            if (actualFilename.length() < 4 || actualFilename.substr(actualFilename.length() - 4) != ".wav") {
                actualFilename += ".wav";
            }
        }

        m_currentFilePath = m_recordingsDir + "/" + actualFilename;
        m_sampleRate = sampleRate;

        // Reset ring buffer
        m_writePos.store(0, std::memory_order_relaxed);
        m_readPos.store(0, std::memory_order_relaxed);
        m_droppedFrames.store(0, std::memory_order_relaxed);

        // Capture start time
        m_recordingStartTime = std::chrono::steady_clock::now();

        // Start recording flag
        m_recording.store(true, std::memory_order_release);

        // Start writer thread
        m_writerThread = std::make_unique<std::thread>(&AudioRecorder::writerThreadFunc, this);

        return m_currentFilePath;
    }

    /**
     * @brief Stop recording and finalize WAV file
     */
    void stopRecording() {
        if (!m_recording.load(std::memory_order_acquire)) {
            return;
        }

        // Signal writer thread to stop
        m_recording.store(false, std::memory_order_release);

        // Wait for writer thread to finish
        if (m_writerThread && m_writerThread->joinable()) {
            m_writerThread->join();
        }
        m_writerThread.reset();

        m_currentFilePath.clear();
    }

    /**
     * @brief Check if currently recording
     */
    bool isRecording() const {
        return m_recording.load(std::memory_order_acquire);
    }

    /**
     * @brief Get current recording file path
     */
    std::string getCurrentFilePath() const {
        return m_currentFilePath;
    }

    /**
     * @brief Push audio frame to ring buffer (RT-safe)
     * @param left Left channel samples
     * @param right Right channel samples
     * @param numSamples Number of samples per channel
     *
     * IMPORTANT: This function is called from the real-time audio thread.
     * It must be lock-free and complete in < 100ns.
     */
    void pushAudioFrame(const float* left, const float* right, size_t numSamples) {
        if (!m_recording.load(std::memory_order_acquire)) {
            return;
        }

        // Get current write position
        size_t writePos = m_writePos.load(std::memory_order_relaxed);
        size_t readPos = m_readPos.load(std::memory_order_acquire);

        // Calculate available space (stereo interleaved)
        const size_t required = numSamples * 2; // stereo
        const size_t available = (readPos <= writePos)
            ? (RING_BUFFER_SIZE - writePos + readPos - 1)
            : (readPos - writePos - 1);

        if (available < required) {
            // Ring buffer full - drop frames (should rarely happen)
            m_droppedFrames.fetch_add(numSamples, std::memory_order_relaxed);
            return;
        }

        // Write interleaved stereo to ring buffer
        for (size_t i = 0; i < numSamples; ++i) {
            m_ringBuffer[writePos] = left[i];
            writePos = (writePos + 1) % RING_BUFFER_SIZE;
            m_ringBuffer[writePos] = right[i];
            writePos = (writePos + 1) % RING_BUFFER_SIZE;
        }

        // Update write position (release semantics for consumer thread)
        m_writePos.store(writePos, std::memory_order_release);
    }

    /**
     * @brief Get number of dropped frames (due to buffer overflow)
     */
    uint64_t getDroppedFrames() const {
        return m_droppedFrames.load(std::memory_order_relaxed);
    }

    /**
     * @brief Get recording duration in seconds
     * @return Duration in seconds, or 0.0 if not recording
     */
    double getRecordingDuration() const {
        if (!m_recording.load(std::memory_order_acquire)) {
            return 0.0;
        }
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_recordingStartTime);
        return elapsed.count() / 1000.0;
    }

private:
    void writerThreadFunc() {
        // Open WAV file
        WAVWriter wavWriter;
        if (!wavWriter.open(m_currentFilePath, m_sampleRate, 2)) { // 2 = stereo
            std::cerr << "Failed to open WAV file: " << m_currentFilePath << std::endl;
            m_recording.store(false, std::memory_order_release);
            return;
        }

        std::cout << "Recording started: " << m_currentFilePath << std::endl;

        // Local buffer for reading from ring buffer
        // Larger batches = fewer write operations = better SD card performance
        // 32768 samples = 16384 frames @ 48kHz stereo = ~341ms per batch
        constexpr size_t BATCH_SIZE = 32768;
        float batch[BATCH_SIZE];

        while (m_recording.load(std::memory_order_acquire)) {
            size_t readPos = m_readPos.load(std::memory_order_relaxed);
            size_t writePos = m_writePos.load(std::memory_order_acquire);

            // Calculate available samples
            const size_t available = (writePos >= readPos)
                ? (writePos - readPos)
                : (RING_BUFFER_SIZE - readPos + writePos);

            if (available == 0) {
                // Buffer empty, sleep briefly
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            // Read batch from ring buffer
            const size_t toRead = std::min(available, BATCH_SIZE);
            for (size_t i = 0; i < toRead; ++i) {
                batch[i] = m_ringBuffer[readPos];
                readPos = (readPos + 1) % RING_BUFFER_SIZE;
            }

            // Update read position (release semantics for producer thread)
            m_readPos.store(readPos, std::memory_order_release);

            // Write to WAV file (non-RT thread, safe to block on disk I/O)
            wavWriter.write(batch, toRead / 2); // toRead is in samples, frames = samples / 2 (stereo)
        }

        // Flush remaining data
        size_t readPos = m_readPos.load(std::memory_order_relaxed);
        size_t writePos = m_writePos.load(std::memory_order_acquire);
        const size_t remaining = (writePos >= readPos)
            ? (writePos - readPos)
            : (RING_BUFFER_SIZE - readPos + writePos);

        if (remaining > 0) {
            float batch[BATCH_SIZE];
            size_t pos = readPos;
            for (size_t i = 0; i < remaining; ++i) {
                batch[i] = m_ringBuffer[pos];
                pos = (pos + 1) % RING_BUFFER_SIZE;
            }
            wavWriter.write(batch, remaining / 2);
        }

        // Close WAV file (finalizes header)
        wavWriter.close();

        const uint64_t dropped = m_droppedFrames.load(std::memory_order_relaxed);
        std::cout << "Recording stopped: " << m_currentFilePath << std::endl;
        std::cout << "Duration: " << wavWriter.getDuration() << " seconds" << std::endl;
        std::cout << "Size: " << wavWriter.getDataSize() << " bytes" << std::endl;
        if (dropped > 0) {
            std::cerr << "Warning: Dropped " << dropped << " frames due to buffer overflow" << std::endl;
        }
    }

    // Ring buffer size: 10 seconds of stereo audio @ 48kHz = 960,000 samples
    static constexpr size_t RING_BUFFER_SIZE = 960000;

    std::string m_recordingsDir;
    std::string m_currentFilePath;
    int m_sampleRate{48000};

    // Lock-free ring buffer (SPSC - single producer, single consumer)
    alignas(64) float m_ringBuffer[RING_BUFFER_SIZE];
    alignas(64) std::atomic<size_t> m_writePos{0}; // Written by RT thread
    alignas(64) std::atomic<size_t> m_readPos{0};  // Written by writer thread

    std::atomic<bool> m_recording{false};
    std::atomic<uint64_t> m_droppedFrames{0};
    std::unique_ptr<std::thread> m_writerThread;
    std::chrono::steady_clock::time_point m_recordingStartTime;
};

} // namespace HoopiPi
