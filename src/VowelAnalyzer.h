#pragma once

#include "VowelModel.h"
#include "LpcFormants.h"
#include <array>
#include <cstddef>

// Independent implementation: 32 ms frames, anti-aliased 8 kHz analysis,
// pre-emphasis, Hamming window, autocorrelation LPC / Levinson-Durbin.
// LPC background: https://praat.org/manual/Sound__To_LPC__autocorrelation____.html
// LPC polynomial roots retain separate formants when spectral peaks merge.
class VowelAnalyzer {
 public:
  VowelAnalyzer() {
    for (size_t i = 0; i < kSamples; ++i)
      window_[i] = 0.54f - 0.46f * std::cos(2 * kPi * i / (kSamples - 1));
  }

  void reset() {
    filled_ = 0;
    result_ = VowelFeatures{};
    candidate_ = held_ = Vowel::Unknown;
    candidateFrames_ = weakFrames_ = 0;
    heldConfidence_ = 0;
  }

  void setModel(const VowelModel& model) { model_ = model; reset(); }
  const VowelModel& model() const { return model_; }

  // Accept the existing 256-pair stereo FFT frame; analyze every second call.
  bool process(const int16_t* stereo, size_t pairs) {
    for (size_t i = 0; i < pairs; ++i) {
      input_[filled_++] = (float(stereo[i * 2]) + stereo[i * 2 + 1]) * 0.5f;
      if (filled_ == input_.size()) { filled_ = 0; analyze(); return true; }
    }
    return false;
  }
  const VowelFeatures& result() const { return result_; }

 private:
  static constexpr float kPi = 3.14159265358979323846f;
  static constexpr size_t kSamples = 256;
  static constexpr size_t kOrder = 10;
  std::array<float, 512> input_{};
  std::array<float, kSamples> mono_{}, work_{}, window_{};
  std::array<float, kOrder + 1> ac_{}, lpc_{}, previous_{};
  LpcFormants formants_;
  size_t filled_ = 0;
  VowelModel model_;
  VowelFeatures result_;
  Vowel candidate_ = Vowel::Unknown, held_ = Vowel::Unknown;
  unsigned candidateFrames_ = 0, weakFrames_ = 0;
  float heldConfidence_ = 0;

  void stabilize(Vowel vowel, float confidence) {
    if (vowel == Vowel::Unknown) {
      candidate_ = Vowel::Unknown;
      candidateFrames_ = 0;
      if (++weakFrames_ >= 3) { held_ = Vowel::Unknown; heldConfidence_ = 0; }
      else heldConfidence_ *= 0.5f;
    } else {
      weakFrames_ = 0;
      if (candidate_ != vowel) { candidate_ = vowel; candidateFrames_ = 1; }
      else ++candidateFrames_;
      if (candidateFrames_ >= 2) { held_ = vowel; heldConfidence_ = confidence; }
      else heldConfidence_ *= 0.7f;
    }
    result_.vowel = held_;
    result_.confidence = heldConfidence_;
  }

  void analyze() {
    ++result_.sequence;
    result_.f1 = result_.f2 = result_.voicing = 0;
    result_.rawConfidence = result_.distance = 0;
    // 15-tap windowed-sinc low-pass (3.4 kHz), then decimate by two.
    // Boundary extension avoids wrapping one end of a frame into the other.
    static constexpr float fir[] = {
      0.0006f, 0.0064f, 0.0069f, -0.0254f, -0.0552f, 0.0572f, 0.2987f,
      0.4216f, 0.2987f, 0.0572f, -0.0552f, -0.0254f, 0.0069f, 0.0064f, 0.0006f};
    float mean = 0;
    for (size_t i = 0; i < kSamples; ++i) {
      float sample = 0;
      for (int tap = 0; tap < 15; ++tap) {
        const int index = std::max(0, std::min(511, int(i * 2) + tap - 7));
        sample += fir[tap] * input_[index];
      }
      mono_[i] = sample;
      mean += sample;
    }
    mean /= kSamples;
    float power = 0;
    for (auto& sample : mono_) { sample -= mean; power += sample * sample; }
    result_.rms = std::sqrt(power / kSamples);
    // Calibration must see features even when the OLD room-noise gate is too
    // high. Its own newly measured quiet level decides which samples to keep.
    if (result_.rms < 20.0f) {
      stabilize(Vowel::Unknown, 0); return;
    }
    // Normalized pitch-lag correlation rejects unvoiced noise / fricatives.
    float voicing = 0;
    for (size_t lag = 20; lag <= 100; ++lag) {  // 80..400 Hz fundamental.
      float xy = 0, xx = 0, yy = 0;
      for (size_t i = lag; i < kSamples; ++i) {
        const float x = mono_[i], y = mono_[i - lag];
        xy += x * y; xx += x * x; yy += y * y;
      }
      voicing = std::max(voicing, xy / std::sqrt(xx * yy + 1e-12f));
    }
    result_.voicing = voicing;
    if (voicing < 0.55f) { stabilize(Vowel::Unknown, 0); return; }
    float previous = 0;
    for (size_t i = 0; i < kSamples; ++i) {
      work_[i] = (mono_[i] - 0.97f * previous) * window_[i];
      previous = mono_[i];
    }
    for (size_t lag = 0; lag <= kOrder; ++lag) {
      float sum = 0;
      for (size_t i = lag; i < kSamples; ++i) sum += work_[i] * work_[i - lag];
      ac_[lag] = sum;
    }
    if (ac_[0] < 1e-6f) { stabilize(Vowel::Unknown, 0); return; }
    for (size_t i = 1; i <= kOrder; ++i) ac_[i] /= ac_[0];
    ac_[0] = 1.0001f;  // diagonal loading for numerical stability.
    lpc_.fill(0); lpc_[0] = 1;
    float error = ac_[0];
    for (size_t order = 1; order <= kOrder; ++order) {
      float reflection = ac_[order];
      for (size_t i = 1; i < order; ++i) reflection += lpc_[i] * ac_[order - i];
      reflection = -reflection / error;
      if (!std::isfinite(reflection) || std::fabs(reflection) >= 0.9999f) {
        stabilize(Vowel::Unknown, 0); return;
      }
      previous_ = lpc_;
      for (size_t i = 1; i < order; ++i)
        lpc_[i] = previous_[i] + reflection * previous_[order - i];
      lpc_[order] = reflection;
      error *= 1 - reflection * reflection;
      if (error < 1e-7f) { stabilize(Vowel::Unknown, 0); return; }
    }
    const auto pair = formants_.extract(lpc_);
    result_.f1 = pair.f1; result_.f2 = pair.f2;
    if (result_.rms < model_.noiseRms * 1.8f) {
      stabilize(Vowel::Unknown, 0); return;
    }
    float confidence = 0;
    const auto vowel = classifyVowel(pair.f1, pair.f2, model_, confidence,
                                     &result_.rawConfidence, &result_.distance);
    stabilize(vowel, confidence);
  }
};
