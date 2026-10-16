#include "audio.h"

#include <algorithm>

bool Mixer::start() {
    WAVEFORMATEX format = {};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 1;
    format.nSamplesPerSec = kRate;
    format.wBitsPerSample = 16;
    format.nBlockAlign = 2;
    format.nAvgBytesPerSec = kRate * 2;
    event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!event_) return false;
    if (waveOutOpen(&device_, WAVE_MAPPER, &format, DWORD_PTR(event_), 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) {
        CloseHandle(event_);
        event_ = nullptr;
        device_ = nullptr;
        return false;
    }
    for (int i = 0; i < kBuffers; ++i) {
        headers_[i].lpData = reinterpret_cast<LPSTR>(buffers_[i]);
        headers_[i].dwBufferLength = sizeof buffers_[i];
        waveOutPrepareHeader(device_, &headers_[i], sizeof(WAVEHDR));
        headers_[i].dwFlags |= WHDR_DONE;  // hand every buffer to the mixer thread first
    }
    running_ = true;
    thread_ = std::thread([this] { run(); });
    return true;
}

void Mixer::stop() {
    if (!running_.exchange(false)) return;
    SetEvent(event_);
    thread_.join();
    waveOutReset(device_);
    for (WAVEHDR& h : headers_) waveOutUnprepareHeader(device_, &h, sizeof(WAVEHDR));
    waveOutClose(device_);
    CloseHandle(event_);
    device_ = nullptr;
    event_ = nullptr;
}

void Mixer::play(int channel, const Sound* sound, int loops) {
    if (channel < 0 || channel >= kChannels) return;
    std::lock_guard<std::mutex> guard(lock_);
    voices_[channel] = {sound, 0.0, loops};
}

void Mixer::flush(int channel) { play(channel, nullptr); }

void Mixer::run() {
    while (running_) {
        for (WAVEHDR& h : headers_) {
            if (!(h.dwFlags & WHDR_DONE)) continue;
            mix(reinterpret_cast<int16_t*>(h.lpData), kBufferFrames);
            h.dwFlags &= ~WHDR_DONE;
            waveOutWrite(device_, &h, sizeof(WAVEHDR));
        }
        WaitForSingleObject(event_, 100);
    }
}

void Mixer::mix(int16_t* out, int frames) {
    std::lock_guard<std::mutex> guard(lock_);
    for (int i = 0; i < frames; ++i) {
        int sum = 0;
        for (Voice& v : voices_) {
            if (!v.sound || v.sound->samples.empty()) continue;
            const auto& s = v.sound->samples;
            const size_t at = size_t(v.position);
            const double frac = v.position - double(at);
            const int a = s[at], b = at + 1 < s.size() ? s[at + 1] : a;
            sum += int(a + (b - a) * frac);
            v.position += double(v.sound->rate) / kRate;
            if (v.position >= double(s.size())) {
                v.position = 0;
                if (v.loops == 0) v.sound = nullptr;
                else if (v.loops > 0) --v.loops;
            }
        }
        out[i] = muted_ ? 0 : int16_t(std::clamp(sum, -32768, 32767));
    }
}
