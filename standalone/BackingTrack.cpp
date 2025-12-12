#include "BackingTrack.h"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace HoopiPi {

BackingTrack::BackingTrack()
    : totalFrames_(0)
    , channels_(0)
    , sampleRate_(0)
    , playbackPosition_(0)
    , isPlaying_(false)
    , loopEnabled_(true)
    , volume_(0.7f)
    , startFrame_(0)
    , stopFrame_(0)
{
}

BackingTrack::~BackingTrack()
{
    unload();
}

bool BackingTrack::loadFile(const std::string& filepath, int jackSampleRate)
{
    // Unload any existing file
    unload();

    // Detect file type by extension
    std::string extension;
    size_t dotPos = filepath.find_last_of('.');
    if (dotPos != std::string::npos) {
        extension = filepath.substr(dotPos + 1);
        // Convert to lowercase for comparison
        std::transform(extension.begin(), extension.end(), extension.begin(),
                      [](unsigned char c) { return std::tolower(c); });
    }

    // Dispatch to appropriate loader
    if (extension == "mp3") {
        return loadMp3File(filepath, jackSampleRate);
    } else {
        // Default to WAV loader (handles .wav, .flac, .ogg, etc. via libsndfile)
        return loadWavFile(filepath, jackSampleRate);
    }
}

bool BackingTrack::loadWavFile(const std::string& filepath, int jackSampleRate)
{
    // Open the WAV file
    SF_INFO sfinfo;
    std::memset(&sfinfo, 0, sizeof(sfinfo));

    SNDFILE* file = sf_open(filepath.c_str(), SFM_READ, &sfinfo);
    if (!file) {
        std::cerr << "[BackingTrack] Failed to open file: " << filepath << std::endl;
        std::cerr << "[BackingTrack] Error: " << sf_strerror(nullptr) << std::endl;
        return false;
    }

    // Validate format
    if (sfinfo.channels < 1 || sfinfo.channels > 2) {
        std::cerr << "[BackingTrack] Unsupported channel count: " << sfinfo.channels << std::endl;
        sf_close(file);
        return false;
    }

    std::cout << "[BackingTrack] Loading: " << filepath << std::endl;
    std::cout << "[BackingTrack]   Channels: " << sfinfo.channels << std::endl;
    std::cout << "[BackingTrack]   Sample rate: " << sfinfo.samplerate << " Hz" << std::endl;
    std::cout << "[BackingTrack]   Frames: " << sfinfo.frames << std::endl;
    std::cout << "[BackingTrack]   Duration: " << (float)sfinfo.frames / sfinfo.samplerate << " seconds" << std::endl;

    // Read interleaved audio data
    std::vector<float> interleavedData(sfinfo.frames * sfinfo.channels);
    sf_count_t framesRead = sf_readf_float(file, interleavedData.data(), sfinfo.frames);
    sf_close(file);

    if (framesRead != sfinfo.frames) {
        std::cerr << "[BackingTrack] Failed to read all frames. Expected: " << sfinfo.frames
                  << ", Read: " << framesRead << std::endl;
        return false;
    }

    // Deinterleave into separate L/R channels
    std::vector<float> tempL(sfinfo.frames);
    std::vector<float> tempR(sfinfo.frames);

    if (sfinfo.channels == 1) {
        // Mono: duplicate to both channels
        for (size_t i = 0; i < (size_t)sfinfo.frames; i++) {
            tempL[i] = interleavedData[i];
            tempR[i] = interleavedData[i];
        }
    } else {
        // Stereo: deinterleave
        for (size_t i = 0; i < (size_t)sfinfo.frames; i++) {
            tempL[i] = interleavedData[i * 2];
            tempR[i] = interleavedData[i * 2 + 1];
        }
    }

    // Resample if needed
    if (sfinfo.samplerate != jackSampleRate) {
        std::cout << "[BackingTrack] Resampling from " << sfinfo.samplerate
                  << " Hz to " << jackSampleRate << " Hz..." << std::endl;

        if (!resampleAudio(tempL, audioDataL_, sfinfo.samplerate, jackSampleRate) ||
            !resampleAudio(tempR, audioDataR_, sfinfo.samplerate, jackSampleRate)) {
            std::cerr << "[BackingTrack] Resampling failed" << std::endl;
            return false;
        }

        totalFrames_ = audioDataL_.size();
        sampleRate_ = jackSampleRate;
    } else {
        // No resampling needed
        audioDataL_ = std::move(tempL);
        audioDataR_ = std::move(tempR);
        totalFrames_ = sfinfo.frames;
        sampleRate_ = sfinfo.samplerate;
    }

    channels_ = sfinfo.channels;
    filename_ = filepath;
    playbackPosition_.store(0);

    // Reset start/stop positions when loading a new file
    startFrame_.store(0);
    stopFrame_.store(0);

    std::cout << "[BackingTrack] File loaded successfully" << std::endl;
    std::cout << "[BackingTrack]   Final frames: " << totalFrames_ << std::endl;
    std::cout << "[BackingTrack]   Final sample rate: " << sampleRate_ << " Hz" << std::endl;

    return true;
}

bool BackingTrack::loadMp3File(const std::string& filepath, int jackSampleRate)
{
    // Initialize mpg123 library
    static bool mpg123Initialized = false;
    if (!mpg123Initialized) {
        int err = mpg123_init();
        if (err != MPG123_OK) {
            std::cerr << "[BackingTrack] Failed to initialize mpg123: " << mpg123_plain_strerror(err) << std::endl;
            return false;
        }
        mpg123Initialized = true;
    }

    // Create mpg123 handle
    int err;
    mpg123_handle* mh = mpg123_new(nullptr, &err);
    if (!mh) {
        std::cerr << "[BackingTrack] Failed to create mpg123 handle: " << mpg123_plain_strerror(err) << std::endl;
        return false;
    }

    // Open the MP3 file
    err = mpg123_open(mh, filepath.c_str());
    if (err != MPG123_OK) {
        std::cerr << "[BackingTrack] Failed to open MP3 file: " << filepath << std::endl;
        std::cerr << "[BackingTrack] Error: " << mpg123_strerror(mh) << std::endl;
        mpg123_delete(mh);
        return false;
    }

    // Get format information
    long mp3SampleRate;
    int mp3Channels, mp3Encoding;
    err = mpg123_getformat(mh, &mp3SampleRate, &mp3Channels, &mp3Encoding);
    if (err != MPG123_OK) {
        std::cerr << "[BackingTrack] Failed to get MP3 format: " << mpg123_strerror(mh) << std::endl;
        mpg123_close(mh);
        mpg123_delete(mh);
        return false;
    }

    // Validate channel count
    if (mp3Channels < 1 || mp3Channels > 2) {
        std::cerr << "[BackingTrack] Unsupported MP3 channel count: " << mp3Channels << std::endl;
        mpg123_close(mh);
        mpg123_delete(mh);
        return false;
    }

    // Set output format to signed 16-bit
    mpg123_format_none(mh);
    mpg123_format(mh, mp3SampleRate, mp3Channels, MPG123_ENC_SIGNED_16);

    // Get total number of samples
    off_t mp3Samples = mpg123_length(mh);
    if (mp3Samples <= 0) {
        std::cerr << "[BackingTrack] Failed to get MP3 length" << std::endl;
        mpg123_close(mh);
        mpg123_delete(mh);
        return false;
    }

    std::cout << "[BackingTrack] Loading MP3: " << filepath << std::endl;
    std::cout << "[BackingTrack]   Channels: " << mp3Channels << std::endl;
    std::cout << "[BackingTrack]   Sample rate: " << mp3SampleRate << " Hz" << std::endl;
    std::cout << "[BackingTrack]   Frames: " << mp3Samples << std::endl;
    std::cout << "[BackingTrack]   Duration: " << (float)mp3Samples / mp3SampleRate << " seconds" << std::endl;

    // Read entire MP3 file into buffer (as 16-bit signed integers)
    std::vector<int16_t> rawData(mp3Samples * mp3Channels);
    size_t totalBytesRead = 0;
    size_t bytesRead;
    unsigned char* bufferPtr = reinterpret_cast<unsigned char*>(rawData.data());
    size_t bufferSize = rawData.size() * sizeof(int16_t);

    while (totalBytesRead < bufferSize) {
        err = mpg123_read(mh, bufferPtr + totalBytesRead, bufferSize - totalBytesRead, &bytesRead);

        // Check for errors first (but not DONE, which is success)
        if (err != MPG123_OK && err != MPG123_DONE && err != MPG123_NEW_FORMAT) {
            std::cerr << "[BackingTrack] Error reading MP3 data: " << mpg123_strerror(mh) << std::endl;
            mpg123_close(mh);
            mpg123_delete(mh);
            return false;
        }

        // Add bytes read (important: do this before checking for DONE!)
        totalBytesRead += bytesRead;

        // Check if done (after adding bytes)
        if (err == MPG123_DONE) {
            break;
        }
    }

    mpg123_close(mh);
    mpg123_delete(mh);

    // Convert 16-bit integers to float and deinterleave
    size_t actualFrames = totalBytesRead / (mp3Channels * sizeof(int16_t));

    std::vector<float> tempL(actualFrames);
    std::vector<float> tempR(actualFrames);

    const float scale = 1.0f / 32768.0f;  // Convert int16 to float [-1.0, 1.0]

    if (mp3Channels == 1) {
        // Mono: duplicate to both channels
        for (size_t i = 0; i < actualFrames; i++) {
            float sample = rawData[i] * scale;
            tempL[i] = sample;
            tempR[i] = sample;
        }
    } else {
        // Stereo: deinterleave
        for (size_t i = 0; i < actualFrames; i++) {
            tempL[i] = rawData[i * 2] * scale;
            tempR[i] = rawData[i * 2 + 1] * scale;
        }
    }

    // Resample if needed
    if (mp3SampleRate != jackSampleRate) {
        std::cout << "[BackingTrack] Resampling from " << mp3SampleRate
                  << " Hz to " << jackSampleRate << " Hz..." << std::endl;

        if (!resampleAudio(tempL, audioDataL_, mp3SampleRate, jackSampleRate) ||
            !resampleAudio(tempR, audioDataR_, mp3SampleRate, jackSampleRate)) {
            std::cerr << "[BackingTrack] Resampling failed" << std::endl;
            return false;
        }

        totalFrames_ = audioDataL_.size();
        sampleRate_ = jackSampleRate;
    } else {
        // No resampling needed
        audioDataL_ = std::move(tempL);
        audioDataR_ = std::move(tempR);
        totalFrames_ = actualFrames;
        sampleRate_ = mp3SampleRate;
    }

    channels_ = mp3Channels;
    filename_ = filepath;
    playbackPosition_.store(0);

    // Reset start/stop positions when loading a new file
    startFrame_.store(0);
    stopFrame_.store(0);

    std::cout << "[BackingTrack] MP3 file loaded successfully" << std::endl;
    std::cout << "[BackingTrack]   Final frames: " << totalFrames_ << std::endl;
    std::cout << "[BackingTrack]   Final sample rate: " << sampleRate_ << " Hz" << std::endl;

    return true;
}

void BackingTrack::unload()
{
    stop();
    audioDataL_.clear();
    audioDataR_.clear();
    totalFrames_ = 0;
    channels_ = 0;
    sampleRate_ = 0;
    filename_.clear();
    playbackPosition_.store(0);
    startFrame_.store(0);
    stopFrame_.store(0);
}

bool BackingTrack::isLoaded() const
{
    return totalFrames_ > 0;
}

void BackingTrack::play()
{
    if (!isLoaded()) {
        std::cerr << "[BackingTrack] Cannot play: no file loaded" << std::endl;
        return;
    }

    // Start playback from the start position
    playbackPosition_.store(startFrame_.load());
    isPlaying_.store(true);
    std::cout << "[BackingTrack] Playing: " << filename_ << " (from " << getStartPosition() << "s)" << std::endl;
}

void BackingTrack::stop()
{
    isPlaying_.store(false);
    // Reset to start position
    playbackPosition_.store(startFrame_.load());
    if (isLoaded()) {
        std::cout << "[BackingTrack] Stopped" << std::endl;
    }
}

void BackingTrack::pause()
{
    isPlaying_.store(false);
    if (isLoaded()) {
        std::cout << "[BackingTrack] Paused at frame " << playbackPosition_.load() << std::endl;
    }
}

bool BackingTrack::isPlaying() const
{
    return isPlaying_.load();
}

void BackingTrack::setLoop(bool enabled)
{
    loopEnabled_.store(enabled);
    std::cout << "[BackingTrack] Loop " << (enabled ? "enabled" : "disabled") << std::endl;
}

bool BackingTrack::isLooping() const
{
    return loopEnabled_.load();
}

void BackingTrack::setVolume(float volume)
{
    volume = std::max(0.0f, std::min(1.0f, volume));
    volume_.store(volume);
    std::cout << "[BackingTrack] Volume set to " << volume << std::endl;
}

float BackingTrack::getVolume() const
{
    return volume_.load();
}

void BackingTrack::fillBuffer(float* outL, float* outR, size_t numFrames)
{
    // RT-safe: no allocations, no I/O, just simple array operations

    if (!isPlaying_.load() || totalFrames_ == 0) {
        // Not playing or no data: output silence
        std::memset(outL, 0, numFrames * sizeof(float));
        std::memset(outR, 0, numFrames * sizeof(float));
        return;
    }

    const float vol = volume_.load();
    const bool loop = loopEnabled_.load();
    const size_t startFrame = startFrame_.load();
    const size_t stopFrame = stopFrame_.load();
    // Determine actual end position: stop position or end of file
    const size_t endFrame = (stopFrame > 0 && stopFrame < totalFrames_) ? stopFrame : totalFrames_;
    size_t pos = playbackPosition_.load();

    for (size_t i = 0; i < numFrames; i++) {
        if (pos >= endFrame) {
            if (loop) {
                // Loop back to start position
                pos = startFrame;
            } else {
                // Reached end, stop playback
                isPlaying_.store(false);
                // Fill remaining buffer with silence
                std::memset(&outL[i], 0, (numFrames - i) * sizeof(float));
                std::memset(&outR[i], 0, (numFrames - i) * sizeof(float));
                playbackPosition_.store(startFrame);
                return;
            }
        }

        outL[i] = audioDataL_[pos] * vol;
        outR[i] = audioDataR_[pos] * vol;
        pos++;
    }

    playbackPosition_.store(pos);
}

std::string BackingTrack::getFilename() const
{
    return filename_;
}

size_t BackingTrack::getTotalFrames() const
{
    return totalFrames_;
}

size_t BackingTrack::getCurrentFrame() const
{
    return playbackPosition_.load();
}

float BackingTrack::getDurationSeconds() const
{
    if (sampleRate_ == 0) return 0.0f;
    return (float)totalFrames_ / sampleRate_;
}

int BackingTrack::getChannels() const
{
    return channels_;
}

int BackingTrack::getSampleRate() const
{
    return sampleRate_;
}

void BackingTrack::setStartPosition(float seconds)
{
    if (sampleRate_ <= 0) {
        std::cerr << "[BackingTrack] Cannot set start position: no file loaded" << std::endl;
        return;
    }

    size_t frame = static_cast<size_t>(seconds * sampleRate_);
    // Clamp to valid range
    if (frame >= totalFrames_) {
        frame = (totalFrames_ > 0) ? (totalFrames_ - 1) : 0;
    }

    startFrame_.store(frame);
    std::cout << "[BackingTrack] Start position set to " << seconds << "s (frame " << frame << ")" << std::endl;
}

void BackingTrack::setStopPosition(float seconds)
{
    if (sampleRate_ <= 0) {
        std::cerr << "[BackingTrack] Cannot set stop position: no file loaded" << std::endl;
        return;
    }

    size_t frame = 0;
    if (seconds > 0.0f) {
        frame = static_cast<size_t>(seconds * sampleRate_);
        // Clamp to valid range
        if (frame > totalFrames_) {
            frame = totalFrames_;
        }
    }
    // 0 means end of file

    stopFrame_.store(frame);
    if (frame == 0) {
        std::cout << "[BackingTrack] Stop position set to end of file" << std::endl;
    } else {
        std::cout << "[BackingTrack] Stop position set to " << seconds << "s (frame " << frame << ")" << std::endl;
    }
}

float BackingTrack::getStartPosition() const
{
    if (sampleRate_ <= 0) {
        return 0.0f;
    }
    return static_cast<float>(startFrame_.load()) / sampleRate_;
}

float BackingTrack::getStopPosition() const
{
    if (sampleRate_ <= 0) {
        return 0.0f;
    }
    size_t stopFrame = stopFrame_.load();
    if (stopFrame == 0) {
        // 0 means end of file
        return getDurationSeconds();
    }
    return static_cast<float>(stopFrame) / sampleRate_;
}

bool BackingTrack::resampleAudio(const std::vector<float>& input, std::vector<float>& output,
                                 int inputRate, int outputRate)
{
    if (input.empty() || inputRate <= 0 || outputRate <= 0) {
        return false;
    }

    // Simple linear interpolation resampling
    double ratio = (double)outputRate / inputRate;
    size_t outputFrames = (size_t)(input.size() * ratio);

    try {
        output.resize(outputFrames);
    } catch (const std::exception& e) {
        std::cerr << "[BackingTrack] Resample error: failed to allocate " << outputFrames
                  << " frames: " << e.what() << std::endl;
        return false;
    }

    for (size_t i = 0; i < outputFrames; i++) {
        double inputPos = i / ratio;
        size_t inputIndex = (size_t)inputPos;
        double frac = inputPos - inputIndex;

        if (inputIndex + 1 < input.size()) {
            // Linear interpolation
            output[i] = input[inputIndex] * (1.0 - frac) + input[inputIndex + 1] * frac;
        } else if (inputIndex < input.size()) {
            // Last sample, no interpolation
            output[i] = input[inputIndex];
        } else {
            output[i] = 0.0f;
        }
    }

    return true;
}

} // namespace HoopiPi
