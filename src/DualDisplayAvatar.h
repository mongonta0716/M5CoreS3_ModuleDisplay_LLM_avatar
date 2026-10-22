#pragma once

#include <MultiDisplayAvatar.h>
#include "VowelModel.h"

class DualDisplayAvatar {
 public:
  bool addDisplay(M5GFX& display, uint16_t reservedBottom = 0);
  void update(uint32_t now);
  void nextExpression();
  void handleCommand(char command);
  void setExpression(m5avatar::Expression expression);
  void setMouthOpenRatio(float ratio);
  void setVowel(Vowel vowel, float confidence);
  void setBalloon(const String& title, const String& detail);

 private:
  m5avatar::MultiDisplayAvatar renderer_;
  m5avatar::Expression expression_ = m5avatar::Expression::Neutral;
  float mouthOpenRatio_ = 0;
  Vowel vowel_ = Vowel::Unknown;
  float vowelConfidence_ = 0;
  bool mouthDemo_ = false;
  uint32_t lastPerfAt_ = 0, perfFrames_ = 0;
  uint64_t perfDrawUs_ = 0;
};
