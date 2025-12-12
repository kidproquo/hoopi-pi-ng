#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <sndfile.h>
#include <mpg123.h>

namespace HoopiPi {

class BackingTrack {
public:
    BackingTrack();
    ~BackingTrack();

    // File management
    bool loadFile(const std::string& filepath, int jackSampleRate);
    void unload();
    bool isLoaded() const;

    // Playback control
    void play();
    void stop();
    void pause();
    bool isPlaying() const;

    // Settings
    void setLoop(bool enabled);
    bool isLooping() const;
    void setVolume(float volume);  // 0.0 to 1.0
    float getVolume() const;
    void setStartPosition(float seconds);  // Set start position in seconds
    void setStopPosition(float seconds);   // Set stop position in seconds (0 = end of file)
    float getStartPosition() const;        // Get start position in seconds
    float getStopPosition() const;         // Get stop position in seconds (0 = end of file)

    // Audio processing (RT-safe, called from audio callback)
    void fillBuffer(float* outL, float* outR, size_t numFrames);

    // Info
    std::string getFilename() const;
    size_t getTotalFrames() const;
    size_t getCurrentFrame() const;
    float getDurationSeconds() const;
    int getChannels() const;
    int getSampleRate() const;

private:
    // Audio data (pre-resampled to JACK sample rate)
    std::vector<float> audioDataL_;
    std::vector<float> audioDataR_;
    size_t totalFrames_;
    int channels_;
    int sampleRate_;

    // Playback state (atomic for RT-safety)
    std::atomic<size_t> playbackPosition_;
    std::atomic<bool> isPlaying_;
    std::atomic<bool> loopEnabled_;
    std::atomic<float> volume_;
    std::atomic<size_t> startFrame_;  // Start position in frames
    std::atomic<size_t> stopFrame_;   // Stop position in frames (0 = end of file)

    // File info
    std::string filename_;

    // Helper methods
    bool resampleAudio(const std::vector<float>& input, std::vector<float>& output,
                      int inputRate, int outputRate);
    bool loadWavFile(const std::string& filepath, int jackSampleRate);
    bool loadMp3File(const std::string& filepath, int jackSampleRate);
};

} // namespace HoopiPi
