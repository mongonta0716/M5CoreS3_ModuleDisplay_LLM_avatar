#pragma once
#include <Arduino.h>
#include "VowelCalibration.h"

template <typename Calibration>
inline void calibrationText(const Calibration& calibration, uint32_t now,
                            const char* retry, String& title, String& detail) {
  const auto status = calibration.status(now);
  if (calibration.active()) {
    static const char* labels[] = {"静かに", "あー", "いー", "うー", "えー", "おー"};
    if (calibration.resting()) {
      title = "息つぎ";
      detail = String("あと") + calibration.secondsLeft(now) + "秒";
    } else if (calibration.step() == 0) {
      title = "声の調整";
      detail = String("静かに ") + calibration.secondsLeft(now) + "秒";
    } else {
      title = String(labels[calibration.step()]) + " " + calibration.round() + "回目";
      detail = String("あと") + calibration.secondsLeft(now) + "秒";
    }
  } else {
    switch (status) {
      case VowelCalibration::Status::Complete: title = "声の調整"; detail = "保存完了"; break;
      case VowelCalibration::Status::Insufficient: title = "音声不足"; detail = retry; break;
      case VowelCalibration::Status::Invalid: title = "母音不安定"; detail = retry; break;
      case VowelCalibration::Status::Cancelled: title = "声の調整"; detail = "中止"; break;
      case VowelCalibration::Status::Reset: title = "声の調整"; detail = "初期値です"; break;
      case VowelCalibration::Status::SaveFailed: title = "保存に失敗"; detail = retry; break;
      default: break;
    }
  }
}
