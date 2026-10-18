#pragma once

#include <M5Unified.h>
#include <array>
#include <atomic>
#include "MicFftAnalyzer.h"
#include "VowelAnalyzer.h"

class MicLipSync {
 public:
  bool begin() {
    // Called only with the HDMI speaker stopped: both own I2S1 and GPIO0.
    if (active_.load()) return true;
    pause();
    auto cfg = M5.Mic.config();
    cfg.sample_rate = MicFftAnalyzer::kSampleRate;
    M5.Mic.config(cfg);
    M5.Mic.setBufferReleaseCallback(this, onRecorded);
    analyzer_.reset(millis());
    vowels_.reset();
    publish(VowelFeatures{});
    ratio_.store(0);
    level_.store(0);
    peak_.store(0);
    ownsPort_ = true;
    active_.store(true);
    if (!M5.Mic.begin() || !queueRecording()) {
      pause();
      Serial.println("Mic FFT: initialization failed");
      return false;
    }
    Serial.println("Mic FFT: ready, 16000 Hz stereo, 256 points, bins 5..60; I2S1 MCLK=0 BCK=34 WS=33 DATA=14");
    return true;
  }

  void pause() {
    // Stop requeuing before end() waits for the capture task to exit.
    active_.store(false);
    // Only release the capture lifecycle we own. Repeated A presses can
    // arrive while the shared port already belongs to the HDMI speaker.
    if (ownsPort_) {
      M5.Mic.end();
      ownsPort_ = false;
    }
    ratio_.store(0);
    publish(VowelFeatures{});
  }

  float mouthRatio() const { return ratio_.load(); }

  VowelFeatures vowelFeatures() {
    portENTER_CRITICAL(&featuresMux_);
    const auto result = features_;
    portEXIT_CRITICAL(&featuresMux_);
    return result;
  }

  void setVowelModel(const VowelModel& model) {
    if (!model.valid()) return;
    const bool resume = ownsPort_;
    pause();  // Model updates never race capture/analysis.
    vowels_.setModel(model);
    if (resume) begin();
  }

  void logStats(uint32_t now) {
    if (now - lastLog_ < 1000) return;
    lastLog_ = now;
    Serial.printf("Mic FFT: %s frames=%u level=%u peak=%u mouth=%.2f\n",
                  active_.load() ? "listening" : "paused",
                  static_cast<unsigned>(frames_.load()),
                  static_cast<unsigned>(level_.load()),
                  static_cast<unsigned>(peak_.load()), ratio_.load());
    const auto f = vowelFeatures();
    Serial.printf("Vowel: %s confidence=%.2f F1=%.0f F2=%.0f rms=%.0f voice=%.2f raw=%.2f distance=%.2f analysis=%u us\n",
                  vowelName(f.vowel), f.confidence, f.f1, f.f2, f.rms, f.voicing,
                  f.rawConfidence, f.distance,
                  static_cast<unsigned>(analysisUs_.load()));
  }

 private:
  std::array<int16_t, MicFftAnalyzer::kSampleCount> samples_{};
  MicFftAnalyzer analyzer_;
  VowelAnalyzer vowels_;
  VowelFeatures features_;
  portMUX_TYPE featuresMux_ = portMUX_INITIALIZER_UNLOCKED;
  std::atomic<uint32_t> analysisUs_{0};
  std::atomic<bool> active_{false};
  std::atomic<float> ratio_{0};
  std::atomic<uint32_t> frames_{0}, level_{0}, peak_{0};
  uint32_t lastLog_ = 0;
  bool ownsPort_ = false;  // Main task only; callbacks never change lifecycle.

  void publish(const VowelFeatures& features) {
    portENTER_CRITICAL(&featuresMux_);
    features_ = features;
    portEXIT_CRITICAL(&featuresMux_);
  }

  bool queueRecording() {
    return M5.Mic.record(samples_.data(), samples_.size(),
                         MicFftAnalyzer::kSampleRate, true);
  }

  static void onRecorded(void* arg, void* data, size_t length) {
    auto& self = *static_cast<MicLipSync*>(arg);
    if (!self.active_.load()) return;
    if (data != self.samples_.data() || length != self.samples_.size()) return;
    // The buffer is released here, so FFT never reads a partly written frame.
    const auto started = micros();
    float ratio = self.analyzer_.process(self.samples_.data(), millis());
    if (self.vowels_.process(self.samples_.data(), self.samples_.size() / 2)) {
      self.publish(self.vowels_.result());
      self.analysisUs_.store(micros() - started);
    }
    // The calibrated room-noise gate also closes the fallback mouth in quiet.
    if (self.vowels_.result().rms < self.vowels_.model().noiseRms * 1.8f) ratio = 0;
    self.ratio_.store(ratio);
    self.level_.store(self.analyzer_.level());
    self.peak_.store(self.analyzer_.peak());
    self.frames_.fetch_add(1);
    // Same-rate record() is permitted from M5Unified's release callback.
    // Do not draw, log, or change audio lifecycle from the capture task.
    if (self.active_.load() && !self.queueRecording()) {
      self.active_.store(false);
      self.ratio_.store(0);
    }
  }
};
