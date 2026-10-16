// Software mixer on waveOut, standing in for WAVMIX16 (channels: open, play, flush, close).
#pragma once

#include <windows.h>
#include <mmsystem.h>

#include <atomic>
#include <mutex>
#include <thread>

#include "wav.h"

class Mixer {
public:
    static constexpr int kChannels = 8;  // WaveMix's channel count
    static constexpr int kRate = 44100;

    ~Mixer() { stop(); }
    bool start();
    void stop();

    // Starts `sound` on `channel`, replacing what was playing there. `loops` < 0 repeats forever.
    void play(int channel, const Sound* sound, int loops = 0);
    void flush(int channel);   // WaveMixFlushChannel
    void setMuted(bool muted) { muted_ = muted; }

private:
    struct Voice {
        const Sound* sound = nullptr;
        double position = 0;
        int loops = 0;
    };
    static constexpr int kBuffers = 4, kBufferFrames = 1024;

    void run();
    void mix(int16_t* out, int frames);

    HWAVEOUT device_ = nullptr;
    HANDLE event_ = nullptr;
    WAVEHDR headers_[kBuffers] = {};
    int16_t buffers_[kBuffers][kBufferFrames] = {};
    Voice voices_[kChannels];
    std::mutex lock_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> muted_{false};
};
