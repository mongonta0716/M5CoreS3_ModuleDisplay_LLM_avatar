#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>

// Recover resonances from polynomial roots instead of assuming each formant
// produces a separate local maximum in the LPC spectrum. Independent bounded
// Durand-Kerner solver; LPC/root-to-frequency background:
// https://praat.org/manual/LPC__To_Formant.html
class LpcFormants {
 public:
  static constexpr size_t kOrder = 10;
  struct Pair { float f1 = 0, f2 = 0; };

  Pair extract(const std::array<float, kOrder + 1>& coefficients) {
    constexpr float pi = 3.14159265358979323846f;
    for (size_t i = 0; i < kOrder; ++i) {
      const float angle = 2 * pi * i / kOrder + 0.17f;
      roots_[i] = std::polar(0.95f, angle);
    }
    for (unsigned iteration = 0; iteration < 80; ++iteration) {
      float largest = 0;
      for (size_t i = 0; i < kOrder; ++i) {
        const auto z = roots_[i];
        auto value = std::complex<float>(coefficients[0], 0);
        for (size_t j = 1; j <= kOrder; ++j) value = value * z + coefficients[j];
        auto denominator = std::complex<float>(1, 0);
        for (size_t j = 0; j < kOrder; ++j) if (i != j) denominator *= z - roots_[j];
        if (std::norm(denominator) < 1e-20f) return Pair{};
        auto delta = value / denominator;
        const float step = std::abs(delta);
        if (!std::isfinite(step)) return Pair{};
        if (step > 0.5f) delta *= 0.5f / step;
        roots_[i] -= delta;
        largest = std::max(largest, std::abs(delta));
      }
      if (largest < 0.0001f) break;
    }
    float coefficientSum = 0;
    for (const auto coefficient : coefficients) coefficientSum += std::fabs(coefficient);
    size_t count = 0;
    for (const auto z : roots_) {
      if (z.imag() <= 0.01f) continue;
      auto residual = std::complex<float>(coefficients[0], 0);
      for (size_t j = 1; j <= kOrder; ++j) residual = residual * z + coefficients[j];
      if (std::abs(residual) > coefficientSum * 0.0001f) return Pair{};
      float radius = std::abs(z);
      if (radius > 1) radius = 1 / radius;
      const float frequency = std::arg(z) * 8000 / (2 * pi);
      const float bandwidth = -std::log(radius) * 8000 / pi;
      // Narrow isolated tones and broad slope-fitting poles are not vowels.
      if (frequency < 180 || frequency > 3300 || bandwidth < 20 || bandwidth > 400) continue;
      frequencies_[count++] = frequency;
    }
    std::sort(frequencies_.begin(), frequencies_.begin() + count);
    Pair result;
    for (size_t i = 0; i < count; ++i) {
      const float frequency = frequencies_[i];
      if (result.f1 == 0 && frequency <= 1200) result.f1 = frequency;
      else if (result.f1 > 0 && frequency >= 500 && frequency - result.f1 >= 150) {
        result.f2 = frequency; break;
      }
    }
    return result;
  }

 private:
  std::array<std::complex<float>, kOrder> roots_{};
  std::array<float, kOrder> frequencies_{};
};
