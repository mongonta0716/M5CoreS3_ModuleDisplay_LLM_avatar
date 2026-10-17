#pragma once

#include <fft.hpp>
#include <algorithm>

// Band aggregation and adaptive gain follow m5stack-avatar-mic (MIT; see
// lib/AvatarMicFFT/LICENSE). One input frame = 256 interleaved stereo pairs.
class MicFftAnalyzer {
 public:
  static constexpr uint32_t kSampleRate = 16000;
  static constexpr size_t kSampleCount = FFT_SIZE * 2;

  void reset(uint32_t now) {
    gainMax_ = 10;
    lastActive_ = now;
    mouth_ = 0;
    level_ = 0;
    peak_ = 0;
  }

  float process(const int16_t* samples, uint32_t now) {
    fft_.exec(samples);
    // 16 kHz / 256 = 62.5 Hz/bin: use 312.5 through 3750 Hz.
    uint64_t sum = 0;
    for (size_t bin = 5; bin <= 60; ++bin) sum += fft_.get(bin);
    level_ = static_cast<uint32_t>(sum >> 11);
    peak_ = 0;
    for (size_t i = 0; i < kSampleCount; ++i) {
      const int value = samples[i];
      peak_ = std::max(peak_, static_cast<uint32_t>(value < 0 ? -value : value));
    }
    float target = static_cast<float>(level_) / gainMax_;
    if (target <= 0.01f) {
      target = 0;
      if (now - lastActive_ >= 500) gainMax_ = 10;
    } else {
      if (target > 1.5f) gainMax_ += 10;
      target = std::min(target, 1.0f);
      lastActive_ = now;
    }
    // Fast attack, slower release; avoid flicker between 16 ms FFT windows.
    mouth_ += (target - mouth_) * (target > mouth_ ? 0.65f : 0.25f);
    if (mouth_ < 0.01f) mouth_ = 0;
    return mouth_;
  }

  uint32_t level() const { return level_; }
  uint32_t peak() const { return peak_; }

 private:
  fft_t fft_;
  uint32_t gainMax_ = 10;
  uint32_t lastActive_ = 0;
  uint32_t level_ = 0;
  uint32_t peak_ = 0;
  float mouth_ = 0;
};
