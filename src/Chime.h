#pragma once

#include <M5Unified.h>
#include <algorithm>
#include <array>
#include <cmath>
#include "AppConfig.h"

class Chime {
 public:
  bool begin() {
    if (ready_) return true;
    auto cfg = speaker_.config();
    // Use M5Unified's CoreS3 codec callback for the internal speaker.
#if USE_MODULEDISPLAY
    cfg.pin_bck = 6;
    cfg.pin_data_out = 13;
    cfg.pin_ws = 0;
    cfg.i2s_port = I2S_NUM_1;
    cfg.sample_rate = kSampleRate;
    cfg.stereo = true;
    cfg.buzzer = false;
    cfg.use_dac = false;
#else
    cfg.sample_rate = kSampleRate;
#endif
    speaker_.config(cfg);
    speaker_.setVolume(128);
    // Keep this immutable buffer alive while the background speaker task plays.
    // A short silent lead lets the HDMI receiver acquire the audio stream.
    makeNote(100, 90, 1318.51f);
    makeNote(210, 110, 880.0f);
    ready_ = speaker_.begin();
    Serial.printf("%s audio: %s, 48000 Hz BCK=%d DATA=%d WS=%d\n",
                  outputName(), ready_ ? "ready" : "initialization failed",
                  cfg.pin_bck, cfg.pin_data_out, cfg.pin_ws);
    return ready_;
  }

  bool play(const char* source) {
    if (!ready_) return false;
    const bool queued = speaker_.playRaw(samples_.data(), samples_.size(),
                                         kSampleRate, false, 1, 0, true);
    Serial.printf("%s chime: %s %s\n", outputName(), source, queued ? "queued" : "failed");
    return queued;
  }

  bool isPlaying() const { return speaker_.isPlaying(); }

  void end() {
    speaker_.end();
    ready_ = false;
  }

 private:
  static constexpr uint32_t kSampleRate = 48000;
  static constexpr size_t kSamplesPerMs = kSampleRate / 1000;
  std::array<int16_t, 340 * kSamplesPerMs> samples_{};
#if USE_MODULEDISPLAY
  m5::Speaker_Class speaker_;
#else
  m5::Speaker_Class& speaker_ = M5.Speaker;
#endif
  bool ready_ = false;

  static const char* outputName() { return USE_MODULEDISPLAY ? "HDMI" : "CoreS3"; }

  void makeNote(size_t startMs, size_t durationMs, float frequency) {
    const size_t count = durationMs * kSamplesPerMs;
    constexpr size_t fade = 4 * kSamplesPerMs;
    for (size_t i = 0; i < count; ++i) {
      const float envelope = std::min(1.0f, std::min(i, count - 1 - i) / float(fade));
      samples_[startMs * kSamplesPerMs + i] = static_cast<int16_t>(
          14000.0f * envelope * sinf(2.0f * PI * frequency * i / kSampleRate));
    }
  }
};
