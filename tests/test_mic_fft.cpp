#include "MicFftAnalyzer.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>

using Samples = std::array<int16_t, MicFftAnalyzer::kSampleCount>;

Samples tone(float frequency, float amplitude) {
  Samples result{};
  for (size_t i = 0; i < FFT_SIZE; ++i) {
    const auto value = static_cast<int16_t>(amplitude * std::sin(
        2 * 3.141592653589793 * frequency * i / MicFftAnalyzer::kSampleRate));
    result[2 * i] = result[2 * i + 1] = value;
  }
  return result;
}

int main() {
  // Stack allocation also verifies that the FFT twiddle tables initialize fully.
  MicFftAnalyzer analyzer;
  Samples silence{};
  analyzer.reset(0);
  assert(analyzer.process(silence.data(), 0) == 0);
  assert(analyzer.level() == 0);

  auto voice = tone(1000, 6000);
  const float first = analyzer.process(voice.data(), 16);
  const uint32_t voiceLevel = analyzer.level();
  assert(first > 0.1f && first <= 1);
  assert(analyzer.peak() == 6000);
  for (uint32_t time = 32; time <= 320; time += 16) {
    const float ratio = analyzer.process(voice.data(), time);
    assert(std::isfinite(ratio) && ratio >= 0 && ratio <= 1);
  }
  for (uint32_t time = 336; time <= 1024; time += 16) {
    analyzer.process(silence.data(), time);
  }
  assert(analyzer.process(silence.data(), 1040) == 0);
  // Half a second of silence resets adaptive gain for the next utterance.
  const float afterSilence = analyzer.process(voice.data(), 1056);
  assert(std::fabs(afterSilence - first) < 0.001f);

  analyzer.reset(0);
  auto bass = tone(62.5f, 6000);
  assert(analyzer.process(bass.data(), 16) < 0.05f);
  assert(analyzer.level() < voiceLevel / 10);
  analyzer.reset(0);
  auto treble = tone(6000, 6000);
  assert(analyzer.process(treble.data(), 16) < 0.05f);
  assert(analyzer.level() < voiceLevel / 10);
  analyzer.reset(0);
  Samples dc;
  dc.fill(6000);
  assert(analyzer.process(dc.data(), 16) < 0.05f);

  // Full-scale input, including INT16_MIN, must not overflow the peak or ratio.
  auto loud = tone(1000, 32767);
  loud[0] = -32768;
  const float ratio = analyzer.process(loud.data(), 32);
  assert(ratio >= 0 && ratio <= 1);
  assert(analyzer.peak() == 32768);
  // Unsigned elapsed-time calculation remains valid across millis() rollover.
  analyzer.reset(UINT32_MAX - 250);
  analyzer.process(voice.data(), UINT32_MAX - 200);
  for (uint32_t i = 0; i < 50; ++i) analyzer.process(silence.data(), i * 16);
  assert(analyzer.process(silence.data(), 900) == 0);
  std::puts("Mic FFT: silence, speech band, out-of-band, gain, clipping and rollover passed");
}
