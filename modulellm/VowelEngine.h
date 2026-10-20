#pragma once

#include "VowelAnalyzer.h"
#include "VowelCalibration.h"
#include "MicFftAnalyzer.h"
#include "VowelWire.h"

// One consumer owns DSP/model/calibration. The audio pipe and UART have no
// callbacks into this object, and calibration is sampled at <=10 Hz.
class VowelEngine {
 public:
  using Status = VowelCalibration::Status;
  void setModel(const VowelModel& model) { analyzer_.setModel(model); fft_.reset(0); }
  const VowelModel& model() const { return analyzer_.model(); }
  void resetAnalysis(uint32_t now) { analyzer_.reset(); fft_.reset(now); open_ = 0; }
  bool process(const int16_t* stereo, uint32_t now) {
    open_ = fft_.process(stereo, now);
    if (!analyzer_.process(stereo, 256)) return false;
    if (analyzer_.result().rms < analyzer_.model().noiseRms * 1.8f) open_ = 0;
    if (now - lastCal_ >= 100) {
      lastCal_ = now;
      if (calibration_.update(now, analyzer_.result())) needsSave_ = true;
    }
    return true;
  }
  void startCalibration(uint32_t now) { needsSave_ = false; calibration_.start(now); lastCal_ = now; }
  void cancel(uint32_t now) { calibration_.cancel(now); needsSave_ = false; }
  bool needsSave() const { return needsSave_; }
  void saved(bool ok, uint32_t now) {
    if (!needsSave_) return;
    if (ok) setModel(calibration_.model());
    else calibration_.setStatus(Status::SaveFailed, now);
    needsSave_ = false;
  }
  void resetModel(bool ok, uint32_t now) {
    needsSave_ = false;
    if (ok) setModel(VowelModel{});
    calibration_.setStatus(ok ? Status::Reset : Status::SaveFailed, now);
  }
  const VowelModel& pendingModel() const { return calibration_.model(); }
  const VowelFeatures& features() const { return analyzer_.result(); }
  vowelWire::Frame frame(uint32_t now, bool muted) const {
    vowelWire::Frame f;
    const auto& result = analyzer_.result();
    const bool quiet = result.rms < analyzer_.model().noiseRms * 1.8f;
    f.text = quiet || muted ? "無音" : result.vowel == Vowel::Unknown ? "不明" : vowelKana(result.vowel);
    f.open = quiet || muted ? 0 : open_;
    f.confidence = quiet || muted ? 0 : result.confidence;
    f.status = unsigned(calibration_.status(now)); f.step = calibration_.step();
    f.round = calibration_.round(); f.rest = calibration_.resting();
    f.seconds = calibration_.active() ? calibration_.secondsLeft(now) : 0;
    return f;
  }
 private:
  VowelAnalyzer analyzer_;
  MicFftAnalyzer fft_;
  VowelCalibration calibration_;
  uint32_t lastCal_ = 0;
  float open_ = 0;
  bool needsSave_ = false;
};
