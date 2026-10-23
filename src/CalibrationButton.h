#pragma once
#include <cstdint>

// Input is M5Unified's debounced button state. Emit one action per gesture.
class CalibrationButton {
 public:
  enum class Action { None, Calibrate, Reset, Rotate };
  static constexpr uint32_t kDoubleClickMs = 350;
  static constexpr uint32_t kHoldMs = 3000;

  Action update(bool pressed, uint32_t now) {
    Action action = Action::None;
    // Delay a single click until a possible second press has been ruled out.
    if (pendingClick_ && now - releasedAt_ >= kDoubleClickMs) {
      pendingClick_ = false;
      action = Action::Calibrate;
    }
    if (pressed && !pressed_) {
      since_ = now;
      resetSent_ = false;
      secondClick_ = pendingClick_;
      pendingClick_ = false;
    }
    const uint32_t elapsed = now - since_;
    if (pressed && !resetSent_ && elapsed >= kHoldMs) {
      resetSent_ = true;
      secondClick_ = false;
      action = Action::Reset;
    } else if (!pressed && pressed_ && !resetSent_) {
      if (elapsed >= kHoldMs) {
        // Handle a release first observed after the long-press threshold too.
        action = Action::Reset;
      } else if (secondClick_) {
        action = Action::Rotate;
      } else {
        pendingClick_ = true;
        releasedAt_ = now;
      }
      secondClick_ = false;
    }
    pressed_ = pressed;
    return action;
  }
 private:
  bool pressed_ = false, resetSent_ = false;
  bool pendingClick_ = false, secondClick_ = false;
  uint32_t since_ = 0, releasedAt_ = 0;
};
