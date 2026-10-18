#pragma once

#include "VowelModel.h"
#include <array>

// Main-task-only guided collection. Existing model is replaced only after all
// five vowels have enough voiced frames and distinct, valid centers.
class VowelCalibration {
 public:
  static constexpr uint32_t kQuietDurationMs = 2000;
  static constexpr uint32_t kVowelDurationMs = 3000;
  static constexpr uint32_t kRestDurationMs = 2000;
  enum class Status { Idle, Running, Complete, Insufficient, Invalid, Cancelled, Reset, SaveFailed };

  void start(uint32_t now) {
    model_ = VowelModel{};
    status_ = Status::Running;
    step_ = 0;
    round_ = 1;
    resting_ = false;
    changedAt_ = now;
    lastSequence_ = UINT32_MAX;
    count_ = 0; counts_.fill(0);
  }

  bool update(uint32_t now, const VowelFeatures& f) {
    if (!active()) return false;
    const uint32_t elapsed = now - changedAt_;
    if (resting_) {
      // Discard breath and the last completed rest frame before collecting
      // the next three-second utterance.
      lastSequence_ = f.sequence;
      if (elapsed >= kRestDurationMs) {
        resting_ = false;
        changedAt_ = now;
      }
      return false;
    }
    if (f.sequence != 0 && f.sequence != lastSequence_) {
      lastSequence_ = f.sequence;
      if (step_ == 0 && elapsed >= 300 && count_ < kCapacity) {
        f1_[0][count_++] = f.rms;
      } else if (step_ > 0 && elapsed >= 500 && count_ < kCapacity &&
                 f.voicing >= 0.65f && f.rms > model_.noiseRms * 1.8f &&
                 f.f1 >= 180 && f.f1 <= 1200 && f.f2 >= 500 && f.f2 <= 3300 &&
                 f.f2 - f.f1 >= 150) {
        f1_[step_ - 1][count_] = f.f1; f2_[step_ - 1][count_] = f.f2; ++count_;
      }
    }
    if (elapsed < (step_ == 0 ? kQuietDurationMs : kVowelDurationMs)) return false;
    if (step_ > 0 && round_ == 1) {
      round_ = 2;
      resting_ = true;
      changedAt_ = now;
      return false;  // Combine both utterances before evaluating this vowel.
    }
    if (count_ < (step_ == 0 ? 8u : 12u)) {
      status_ = Status::Insufficient; changedAt_ = now; return false;
    }
    if (step_ == 0) {
      std::sort(f1_[0].begin(), f1_[0].begin() + count_);
      model_.noiseRms = std::max(1.0f, f1_[0][count_ / 2]);
    } else {
      counts_[step_ - 1] = count_;
      if (!summarize(step_ - 1)) {
        status_ = Status::Invalid; changedAt_ = now; return false;
      }
    }
    changedAt_ = now;
    count_ = 0;
    round_ = 1;
    if (++step_ > 5) {
      status_ = validateSamples() ? Status::Complete : Status::Invalid;
      return status_ == Status::Complete;
    }
    resting_ = step_ > 1;  // Also allow a breath between different vowels.
    return false;
  }

  void cancel(uint32_t now) { if (active()) setStatus(Status::Cancelled, now); }
  void setStatus(Status status, uint32_t now) { status_ = status; changedAt_ = now; }
  bool active() const { return status_ == Status::Running; }
  Status status(uint32_t now) const {
    return active() || now - changedAt_ < 6000 ? status_ : Status::Idle;
  }
  unsigned step() const { return step_; }
  unsigned round() const { return round_; }
  bool resting() const { return resting_; }
  unsigned sampleCount() const { return count_; }
  Vowel expected() const { return active() && step_ > 0 && !resting_ ? Vowel(step_) : Vowel::Unknown; }
  unsigned secondsLeft(uint32_t now) const {
    const uint32_t duration = resting_ ? kRestDurationMs :
                             (step_ == 0 ? kQuietDurationMs : kVowelDurationMs);
    const uint32_t elapsed = now - changedAt_;
    return elapsed < duration ? (duration - elapsed + 999) / 1000 : 0;
  }
  const VowelModel& model() const { return model_; }

 private:
  static constexpr unsigned kCapacity = 64;
  Status status_ = Status::Idle;
  VowelModel model_;
  unsigned step_ = 0, round_ = 1, count_ = 0;
  bool resting_ = false;
  uint32_t changedAt_ = 0, lastSequence_ = UINT32_MAX;
  std::array<std::array<float, kCapacity>, 5> f1_{}, f2_{};
  std::array<unsigned, 5> counts_{};
  std::array<float, kCapacity> sorted_{};

  bool summarize(unsigned vowel) {
    sorted_ = f1_[vowel];
    std::sort(sorted_.begin(), sorted_.begin() + count_);
    const float median1 = sorted_[count_ / 2];
    sorted_ = f2_[vowel];
    std::sort(sorted_.begin(), sorted_.begin() + count_);
    const float median2 = sorted_[count_ / 2];
    float closest = 1e9f;
    unsigned inliers = 0, representative = 0;
    for (unsigned i = 0; i < count_; ++i) {
      const float x = std::log(f1_[vowel][i] / median1) / 0.3f;
      const float y = std::log(f2_[vowel][i] / median2) / 0.22f;
      const float distance = x * x + y * y;
      if (distance <= 1) ++inliers;
      if (distance < closest) { closest = distance; representative = i; }
    }
    if (inliers < 12 || inliers < count_ * 0.65f) return false;
    // Preserve an observed pair, rather than independently combining two
    // medians that may have come from different resonance assignments.
    model_.f1[vowel] = f1_[vowel][representative];
    model_.f2[vowel] = f2_[vowel][representative];
    return true;
  }

  bool validateSamples() const {
    if (!model_.valid()) return false;
    for (unsigned vowel = 0; vowel < 5; ++vowel) {
      unsigned correct = 0;
      for (unsigned i = 0; i < counts_[vowel]; ++i) {
        float confidence = 0;
        correct += classifyVowel(f1_[vowel][i], f2_[vowel][i], model_, confidence) == Vowel(vowel + 1);
      }
      if (correct < 12 || correct < counts_[vowel] * 0.65f) return false;
    }
    return true;
  }
};
