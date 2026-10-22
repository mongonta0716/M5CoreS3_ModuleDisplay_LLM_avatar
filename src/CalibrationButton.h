#pragma once
#include <cstdint>

// Input is M5Unified's debounced button state. Emit one action per gesture.
class CalibrationButton {
 public:
  enum class Action { None, Calibrate, Reset };
  Action update(bool pressed, uint32_t now) {
    Action action = Action::None;
    if (pressed && !pressed_) {
      since_ = now;
      resetSent_ = false;
    }
    const uint32_t elapsed = now - since_;
    if (pressed && !resetSent_ && elapsed >= 3000) {
      resetSent_ = true;
      action = Action::Reset;
    } else if (!pressed && pressed_ && !resetSent_) {
      // Also handle a release first observed after the threshold.
      action = elapsed >= 3000 ? Action::Reset : Action::Calibrate;
    }
    pressed_ = pressed;
    return action;
  }
 private:
  bool pressed_ = false, resetSent_ = false;
  uint32_t since_ = 0;
};
