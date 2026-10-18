#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

enum class Vowel : uint8_t { Unknown, A, I, U, E, O };

inline const char* vowelName(Vowel v) {
  static const char* names[] = {"-", "a", "i", "u", "e", "o"};
  const unsigned index = static_cast<unsigned>(v);
  return names[index < 6 ? index : 0];
}

inline const char* vowelKana(Vowel v) {
  static const char* names[] = {"…", "あ", "い", "う", "え", "お"};
  const unsigned index = static_cast<unsigned>(v);
  return names[index < 6 ? index : 0];
}

struct VowelModel {
  // Initial tunable centers, not universal Japanese vowel boundaries.
  float f1[5] = {800, 300, 350, 500, 500};
  float f2[5] = {1200, 2300, 1300, 1900, 850};
  float noiseRms = 35;

  bool valid() const {
    if (!std::isfinite(noiseRms) || noiseRms < 1 || noiseRms > 3000) return false;
    for (int i = 0; i < 5; ++i) {
      if (!std::isfinite(f1[i]) || !std::isfinite(f2[i]) ||
          f1[i] < 180 || f1[i] > 1200 || f2[i] < 500 || f2[i] > 3300 ||
          f2[i] - f1[i] < 150) return false;
      for (int j = 0; j < i; ++j) {
        const float d1 = std::log(f1[i] / f1[j]);
        const float d2 = std::log(f2[i] / f2[j]);
        if (d1 * d1 + d2 * d2 < 0.015f) return false;
      }
    }
    return true;
  }
};

struct VowelFeatures {
  uint32_t sequence = 0;
  float f1 = 0, f2 = 0;
  float rms = 0, voicing = 0, confidence = 0;
  float rawConfidence = 0, distance = 0;
  Vowel vowel = Vowel::Unknown;
};

inline Vowel classifyVowel(float f1, float f2, const VowelModel& model,
                           float& confidence, float* rawConfidence = nullptr,
                           float* distanceOut = nullptr) {
  confidence = 0;
  if (rawConfidence) *rawConfidence = 0;
  if (distanceOut) *distanceOut = 0;
  if (!(f1 >= 180 && f1 <= 1200 && f2 >= 500 && f2 <= 3300 && f2 - f1 >= 150))
    return Vowel::Unknown;
  float best = 1e9f, second = 1e9f;
  unsigned nearest = 0;
  for (unsigned i = 0; i < 5; ++i) {
    const float x = std::log(f1 / model.f1[i]) / 0.35f;
    const float y = std::log(f2 / model.f2[i]) / 0.25f;
    const float distance = x * x + y * y;
    if (distance < best) { second = best; best = distance; nearest = i; }
    else second = std::min(second, distance);
  }
  confidence = std::exp(-0.5f * best) * (second - best) / (second + 0.1f);
  if (rawConfidence) *rawConfidence = confidence;
  if (distanceOut) *distanceOut = best;
  if (best > 4 || confidence < 0.25f) { confidence = 0; return Vowel::Unknown; }
  return static_cast<Vowel>(nearest + 1);
}

struct MouthPose {
  float width = 90, height = 4, roundness = 0;
  Vowel label = Vowel::Unknown;
};

inline MouthPose vowelMouthPose(Vowel vowel, float confidence, float open) {
  open = std::max(0.0f, std::min(1.0f, open));
  confidence = std::max(0.0f, std::min(1.0f, confidence));
  MouthPose pose;
  pose.width = 90 - 40 * open;
  pose.height = 4 + 56 * open;
  const unsigned index = static_cast<unsigned>(vowel);
  if (index == 0 || index > 5) return pose;
  static const float widths[] = {66, 96, 28, 92, 42};
  static const float heights[] = {78, 12, 28, 34, 58};
  static const float rounds[] = {0.3f, 0.3f, 1, 0.15f, 1};
  // Accepted labels should keep their identity even at moderate confidence.
  // Weak/decaying labels still fade back to amplitude-only motion.
  const float strength = confidence >= 0.25f ?
      0.85f + 0.15f * (confidence - 0.25f) / 0.75f : confidence * 3.4f;
  const float closure = std::min(1.0f, open / 0.06f);
  const float energy = closure * (0.3f + 0.7f * std::sqrt(open));
  // Width and corner shape identify the vowel; volume mainly changes height.
  const float width = 90 + (widths[index - 1] - 90) * closure;
  const float height = 4 + (heights[index - 1] - 4) * energy;
  pose.width += (width - pose.width) * strength;
  pose.height += (height - pose.height) * strength;
  pose.roundness = rounds[index - 1] * strength * closure;
  // A valid quiet vowel should not turn into ellipsis just because the
  // adaptive amplitude controller opens the mouth less than three percent.
  if (confidence >= 0.25f && open > 0) pose.label = vowel;
  return pose;
}

inline void smoothMouthPose(MouthPose& pose, const MouthPose& wanted, uint32_t elapsed) {
  const float smoothing = 1 - std::exp(-float(std::min(250u, elapsed)) / 70.0f);
  pose.width += (wanted.width - pose.width) * smoothing;
  pose.height += (wanted.height - pose.height) * smoothing;
  pose.roundness += (wanted.roundness - pose.roundness) * smoothing;
  pose.label = wanted.label;
}
