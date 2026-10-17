#pragma once

#include <M5Unified.h>
#include <Face.h>
#include <memory>

// Render both faces sequentially with one animation state. The library's
// Avatar::start() uses a global draw-task handle, unsuitable for two instances.
class DualDisplayAvatar {
 public:
  bool addDisplay(M5GFX& display, uint16_t reservedBottom = 0);
  void update(uint32_t now);
  void nextExpression();
  void handleCommand(char command);
  void setExpression(m5avatar::Expression expression);
  void setMouthOpenRatio(float ratio);

 private:
  struct Target {
    std::unique_ptr<m5avatar::Face> face;
    // Face owns the canvases; keep these pointers only to check allocations.
    M5Canvas* canvas = nullptr;
    M5Canvas* strip = nullptr;
    M5GFX* display = nullptr;
    int viewportHeight = 0;
    int size = 0;
    float scale = 1.0f;
    bool ready = false;
  };

  Target targets_[2];
  uint8_t targetCount_ = 0;
  m5avatar::ColorPalette palette_;
  m5avatar::Expression expression_ = m5avatar::Expression::Neutral;
  float mouthOpenRatio_ = 0.0f;
  float gazeV_ = 0.0f;
  float gazeH_ = 0.0f;
  uint32_t lastFrame_ = 0;
  uint32_t lastGaze_ = 0;
  uint32_t lastBlink_ = 0;
  uint32_t blinkInterval_ = 2500;
  bool mouthDemo_ = false;
  uint32_t lastPerfAt_ = 0;
  uint32_t perfFrames_ = 0;
  uint64_t perfDrawUs_ = 0;
};
