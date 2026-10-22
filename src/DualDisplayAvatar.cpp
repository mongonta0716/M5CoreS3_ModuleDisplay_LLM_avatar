#include "DualDisplayAvatar.h"

#include "AppConfig.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr m5avatar::Expression kExpressions[] = {
    m5avatar::Expression::Neutral, m5avatar::Expression::Happy,
    m5avatar::Expression::Angry, m5avatar::Expression::Sad,
    m5avatar::Expression::Doubt, m5avatar::Expression::Sleepy};
constexpr const char* kExpressionNames[] = {
    "Neutral", "Happy", "Angry", "Sad", "Doubt", "Sleepy"};
}  // namespace

bool DualDisplayAvatar::addDisplay(M5GFX& display, uint16_t reservedBottom) {
  renderer_.setFrameInterval(app::kFrameIntervalMs);
  const bool ready = renderer_.addDisplay(display, reservedBottom);
  if (ready) {
    Serial.printf("Avatar ready: %dx%d; bottom %u px reserved (%u displays)\n",
                  display.width(), display.height(), reservedBottom,
                  static_cast<unsigned>(renderer_.getDisplayCount()));
  } else {
    Serial.println("ERROR: avatar display setup failed.");
  }
  return ready;
}

void DualDisplayAvatar::setExpression(m5avatar::Expression expression) {
  expression_ = expression;
  renderer_.setExpression(expression);
  for (unsigned i = 0; i < sizeof(kExpressions) / sizeof(kExpressions[0]); ++i) {
    if (kExpressions[i] == expression) {
      Serial.printf("Expression: %s\n", kExpressionNames[i]);
      break;
    }
  }
}

void DualDisplayAvatar::nextExpression() {
  constexpr unsigned count = sizeof(kExpressions) / sizeof(kExpressions[0]);
  for (unsigned i = 0; i < count; ++i) {
    if (expression_ == kExpressions[i]) {
      setExpression(kExpressions[(i + 1) % count]);
      return;
    }
  }
}

void DualDisplayAvatar::setMouthOpenRatio(float ratio) {
  mouthOpenRatio_ = std::max(0.0f, std::min(1.0f, ratio));
}

void DualDisplayAvatar::setVowel(Vowel vowel, float confidence) {
  vowel_ = vowel;
  vowelConfidence_ = std::max(0.0f, std::min(1.0f, confidence));
}

void DualDisplayAvatar::setBalloon(const String& title, const String& detail) {
  renderer_.setBalloon(title, detail);
}

void DualDisplayAvatar::handleCommand(char command) {
  switch (command) {
    case 'n': setExpression(m5avatar::Expression::Neutral); break;
    case 'h': setExpression(m5avatar::Expression::Happy); break;
    case 'a': setExpression(m5avatar::Expression::Angry); break;
    case 's': setExpression(m5avatar::Expression::Sad); break;
    case 'd': setExpression(m5avatar::Expression::Doubt); break;
    case 'z': setExpression(m5avatar::Expression::Sleepy); break;
    case 't':
      mouthDemo_ = !mouthDemo_;
      setMouthOpenRatio(0);
      Serial.printf("Mouth demo: %s\n", mouthDemo_ ? "on" : "off");
      break;
    default: break;
  }
}

void DualDisplayAvatar::update(uint32_t now) {
  const uint32_t startedAt = micros();
  if (lastPerfAt_ == 0) lastPerfAt_ = millis();
  const float mouth = mouthDemo_
      ? fabsf(sinf((now % 600) * (2.0f * PI / 600.0f))) : mouthOpenRatio_;
  renderer_.setMouthOpenRatio(mouth);
  // The DSP enum deliberately stays independent of the rendering library.
  m5avatar::Vowel label = m5avatar::Vowel::Unknown;
  if (!mouthDemo_) {
    switch (vowel_) {
      case Vowel::A: label = m5avatar::Vowel::A; break;
      case Vowel::I: label = m5avatar::Vowel::I; break;
      case Vowel::U: label = m5avatar::Vowel::U; break;
      case Vowel::E: label = m5avatar::Vowel::E; break;
      case Vowel::O: label = m5avatar::Vowel::O; break;
      default: break;
    }
  }
  renderer_.setVowel(label, vowelConfidence_);
  if (!renderer_.update(now)) return;
  perfDrawUs_ += static_cast<uint32_t>(micros() - startedAt);
  ++perfFrames_;
  const uint32_t elapsed = millis() - lastPerfAt_;
  if (elapsed >= 5000) {
    Serial.printf("Avatar perf: %.1f fps, mean draw %.1f ms (%u displays)\n",
                  perfFrames_ * 1000.0f / elapsed,
                  perfDrawUs_ / (1000.0f * perfFrames_),
                  static_cast<unsigned>(renderer_.getDisplayCount()));
    lastPerfAt_ = millis();
    perfFrames_ = 0;
    perfDrawUs_ = 0;
  }
}
