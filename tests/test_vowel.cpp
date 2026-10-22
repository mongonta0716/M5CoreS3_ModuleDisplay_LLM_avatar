#include "VowelAnalyzer.h"
#include "VowelCalibration.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <vector>

std::vector<int16_t> synth(float f1, float f2, float pitch) {
  constexpr int rate = 16000, count = 16000;
  const float frequencies[] = {f1, f2, 2900};
  float a[3], b[3], z1[3]{}, z2[3]{};
  for (int j = 0; j < 3; ++j) {
    const float r = std::exp(-3.14159265f * (j == 2 ? 140 : 90) / rate);
    a[j] = 2 * r * std::cos(2 * 3.14159265f * frequencies[j] / rate);
    b[j] = -r * r;
  }
  std::vector<float> raw(count);
  float peak = 0, phase = 0;
  for (int i = 0; i < count; ++i) {
    phase += pitch / rate;
    float x = 0;
    if (phase >= 1) { phase -= 1; x = 1; }
    for (int j = 0; j < 3; ++j) {
      const float y = x + a[j] * z1[j] + b[j] * z2[j];
      z2[j] = z1[j]; z1[j] = y; x = y;
    }
    raw[i] = x;
    peak = std::max(peak, std::fabs(x));
  }
  std::vector<int16_t> pcm(count * 2);
  for (int i = 0; i < count; ++i) pcm[i * 2] = pcm[i * 2 + 1] = raw[i] / peak * 12000;
  return pcm;
}

int main() {
  // Two overlapping resonances still have two polynomial pole pairs. A local
  // maximum picker can collapse them into one peak and mistake F3 for F2.
  for (auto pair : {std::array<float, 2>{{750, 950}}, std::array<float, 2>{{300, 600}}}) {
    std::array<double, 11> coefficients{};
    coefficients[0] = 1;
    unsigned degree = 0;
    const float frequencies[] = {pair[0], pair[1], 2500, 3200, 3700};
    const float bandwidths[] = {160, 160, 140, 900, 1200};
    for (unsigned j = 0; j < 5; ++j) {
      const double r = std::exp(-3.141592653589793 * bandwidths[j] / 8000);
      const double b = -2 * r * std::cos(2 * 3.141592653589793 * frequencies[j] / 8000);
      std::array<double, 11> next{};
      for (unsigned k = 0; k <= degree; ++k) {
        next[k] += coefficients[k]; next[k + 1] += coefficients[k] * b;
        next[k + 2] += coefficients[k] * r * r;
      }
      coefficients = next; degree += 2;
    }
    std::array<float, 11> lpc;
    std::copy(coefficients.begin(), coefficients.end(), lpc.begin());
    LpcFormants roots;
    const auto result = roots.extract(lpc);
    assert(std::fabs(result.f1 - pair[0]) < 30);
    assert(std::fabs(result.f2 - pair[1]) < 30);
  }
  VowelModel model;
  assert(model.valid());
  for (int i = 0; i < 5; ++i) {
    float confidence = 0;
    assert(classifyVowel(model.f1[i], model.f2[i], model, confidence) == Vowel(i + 1));
    assert(confidence > 0.8f);
    for (float pitch : {100.0f, 150.0f, 220.0f}) {
      VowelAnalyzer analyzer;
      auto pcm = synth(model.f1[i], model.f2[i], pitch);
      int correct = 0, total = 0;
      for (size_t offset = 0; offset + 512 <= pcm.size(); offset += 512) {
        if (!analyzer.process(pcm.data() + offset, 256)) continue;
        if (offset > 4000) {
          ++total;
          correct += analyzer.result().vowel == Vowel(i + 1);
        }
      }
      const auto f = analyzer.result();
      std::printf("%s pitch %.0f: F1 %.0f F2 %.0f voice %.2f confidence %.2f -> %s (%d/%d)\n",
                  vowelName(Vowel(i + 1)), pitch, f.f1, f.f2, f.voicing,
                  f.confidence, vowelName(f.vowel), correct, total);
      assert(correct >= total * 0.7f);
    }
  }
  VowelAnalyzer analyzer;
  VowelModel oldNoisyModel;
  oldNoisyModel.noiseRms = 3000;
  analyzer.setModel(oldNoisyModel);
  const auto quietVowel = synth(300, 2300, 150);
  for (size_t offset = 0; offset + 512 <= quietVowel.size(); offset += 512)
    analyzer.process(quietVowel.data() + offset, 256);
  assert(analyzer.result().rms < oldNoisyModel.noiseRms * 1.8f);
  assert(analyzer.result().vowel == Vowel::Unknown);
  assert(analyzer.result().f1 > 0 && analyzer.result().f2 > 0);  // available to calibration.
  analyzer.setModel(VowelModel{});
  std::array<int16_t, 512> samples{};
  for (int i = 0; i < 20; ++i) analyzer.process(samples.data(), 256);
  assert(analyzer.result().vowel == Vowel::Unknown);
  for (int frame = 0; frame < 30; ++frame) {
    for (size_t n = 0; n < 256; ++n) {
      samples[2*n] = samples[2*n+1] = 6000 * std::sin(
          2 * 3.14159265 * 1000 * (frame * 256 + n) / 16000);
    }
    analyzer.process(samples.data(), 256);
  }
  assert(analyzer.result().vowel == Vowel::Unknown);  // one tone is not a vowel.
  // Deterministic broadband noise must not acquire a confident vowel.
  uint32_t random = 42;
  for (int i = 0; i < 20; ++i) {
    for (size_t n = 0; n < 256; ++n) {
      random = random * 1664525u + 1013904223u;
      samples[2*n] = samples[2*n+1] = int16_t(random >> 16) / 2;
    }
    analyzer.process(samples.data(), 256);
    assert(analyzer.result().vowel == Vowel::Unknown);
  }
  auto invalid = model; invalid.f1[0] = NAN;
  assert(!invalid.valid());
  // Guided collection accepts unique completed frames, handles time rollover,
  // and produces a model only after all five vowels are present.
  VowelCalibration calibration;
  const uint32_t start = UINT32_MAX - 1000;
  calibration.start(start);
  bool complete = false;
  VowelFeatures features;
  for (uint32_t elapsed = 0; elapsed <= 50000; elapsed += 100) {
    ++features.sequence;
    features.rms = calibration.step() == 0 ? 40 : 1000;
    features.voicing = 0.95f;
    const int vowel = std::max(0, int(calibration.step()) - 1);
    features.f1 = model.f1[vowel]; features.f2 = model.f2[vowel];
    if (calibration.resting()) {
      features.f1 = 1100; features.f2 = 3250;  // Breath must not enter the model.
    }
    complete = calibration.update(start + elapsed, features) || complete;
    if (elapsed == 5000) {
      assert(calibration.step() == 1 && calibration.round() == 2);
      assert(calibration.resting() && calibration.secondsLeft(start + elapsed) == 2);
      assert(!complete);
    }
    if (elapsed == 7000) {
      assert(calibration.step() == 1 && calibration.round() == 2);
      assert(!calibration.resting() && calibration.secondsLeft(start + elapsed) == 3);
    }
    if (elapsed == 10000) {
      assert(calibration.step() == 2 && calibration.round() == 1 && calibration.resting());
    }
  }
  assert(complete && !calibration.active() && calibration.model().valid());
  assert(calibration.model().noiseRms == 40);
  for (int v = 0; v < 5; ++v) {
    assert(calibration.model().f1[v] == model.f1[v]);
    assert(calibration.model().f2[v] == model.f2[v]);
  }
  calibration.start(0);
  features.sequence = 1;
  for (uint32_t time = 0; time <= 2200; time += 100) calibration.update(time, features);
  assert(calibration.status(2200) == VowelCalibration::Status::Insufficient);
  calibration.start(0); calibration.cancel(100);
  assert(!calibration.active());
  assert(calibration.status(100) == VowelCalibration::Status::Cancelled);
  // Brief room-noise spikes must not inflate the noise gate; sporadic F3
  // assignments must not displace the vowel's representative observed pair.
  calibration.start(0);
  complete = false;
  for (uint32_t time = 0; time <= 50000; time += 100) {
    ++features.sequence;
    const unsigned step = calibration.step();
    const unsigned v = step == 0 ? 0 : step - 1;
    features.rms = step == 0 ? (time < 500 ? 1000 : 40) : 1000;
    features.voicing = 0.95f;
    features.f1 = model.f1[v];
    features.f2 = time % 500 == 0 && step > 0 ? 3000 : model.f2[v];
    complete = calibration.update(time, features) || complete;
  }
  assert(complete && calibration.model().noiseRms == 40);
  assert(calibration.model().f2[0] == model.f2[0]);
  // A sustained split between F2 and F3 is unstable, and must not be saved.
  calibration.start(0);
  for (uint32_t time = 0; time <= 10000; time += 100) {
    ++features.sequence;
    features.rms = calibration.step() == 0 ? 40 : 1000;
    features.f1 = 800;
    features.f2 = time % 200 == 0 ? 1250 : 2575;
    calibration.update(time, features);
  }
  assert(calibration.status(10000) == VowelCalibration::Status::Invalid);
  invalid = model; invalid.f1[0] = model.f1[1]; invalid.f2[0] = model.f2[1];
  assert(!invalid.valid());
  std::puts("Vowel DSP and calibration tests passed");
}
