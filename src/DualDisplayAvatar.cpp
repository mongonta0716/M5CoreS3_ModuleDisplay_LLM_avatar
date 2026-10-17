#include "DualDisplayAvatar.h"

#include "AppConfig.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr int kSourceWidth = 320;
constexpr int kSourceHeight = 240;
constexpr int kStripHeight = 8;
constexpr m5avatar::Expression kExpressions[] = {
    m5avatar::Expression::Neutral, m5avatar::Expression::Happy,
    m5avatar::Expression::Angry, m5avatar::Expression::Sad,
    m5avatar::Expression::Doubt, m5avatar::Expression::Sleepy};
constexpr const char* kExpressionNames[] = {
    "Neutral", "Happy", "Angry", "Sad", "Doubt", "Sleepy"};
}  // namespace

bool DualDisplayAvatar::addDisplay(M5GFX& display, uint16_t reservedBottom) {
  if (targetCount_ >= 2 || reservedBottom >= display.height()) return false;
  const int viewportHeight = display.height() - reservedBottom;
  display.setColorDepth(16);
  display.fillRect(0, 0, display.width(), viewportHeight, TFT_BLACK);

  const float scale = std::min(display.width() / float(kSourceWidth),
                               viewportHeight / float(kSourceHeight));
  const int size = lroundf(kSourceWidth * scale);
  // Face::draw uses a square rotation canvas. Center the 4:3 face in that
  // square, then clip its unused top/bottom to the display's avatar viewport.
  const int contentHeight = lroundf(kSourceHeight * scale);
  const int paddingY = (size - contentHeight) / 2;
  auto position = [scale, paddingY](int top, int left) {
    return new m5avatar::BoundingRect(lroundf(top * scale) + paddingY,
                                       lroundf(left * scale));
  };
  auto dimension = [scale](int value) {
    return static_cast<uint16_t>(std::max(1L, lroundf(value * scale)));
  };

  auto canvas = std::unique_ptr<M5Canvas>(new M5Canvas(&display));
  auto strip = std::unique_ptr<M5Canvas>(new M5Canvas(&display));
  canvas->setColorDepth(1);
  canvas->setPsram(true);
  strip->setColorDepth(16);
  strip->setPsram(false);  // DMA transfer buffer stays in internal RAM.
  if (!canvas->createSprite(size, size) ||
      !strip->createSprite(size, kStripHeight)) {
    Serial.printf("ERROR: avatar allocation failed for %dx%d. "
                  "Try AVATAR_HDMI_SCALE=2 or 4.\n",
                  display.width(), display.height());
    display.setTextColor(TFT_RED, TFT_BLACK);
    display.setCursor(0, 0);
    display.println("Avatar: insufficient memory");
    return false;
  }

  auto& target = targets_[targetCount_];
  target.canvas = canvas.get();
  target.strip = strip.get();
  target.display = &display;
  target.viewportHeight = viewportHeight;
  target.size = size;
  target.scale = scale;
  target.face.reset(new m5avatar::Face(
      new m5avatar::Mouth(dimension(50), dimension(90), dimension(4), dimension(60)),
      position(148, 163),
      new m5avatar::Eye(dimension(8), false), position(93, 90),
      new m5avatar::Eye(dimension(8), true), position(96, 230),
      new m5avatar::Eyeblow(dimension(32), 0, false), position(67, 96),
      new m5avatar::Eyeblow(dimension(32), 0, true), position(72, 230),
      new m5avatar::BoundingRect((viewportHeight - size) / 2,
                                 (display.width() - size) / 2, size, size),
      canvas.release(), strip.release()));
  target.ready = true;
  ++targetCount_;
  Serial.printf("Avatar ready: %dx%d, face %dx%d, 1-bit canvas %u bytes\n",
                display.width(), display.height(), size, contentHeight,
                static_cast<unsigned>(((size + 7) / 8) * size));
  if (reservedBottom) {
    Serial.printf("Avatar viewport: %dx%d; bottom %u px reserved for buttons\n",
                  display.width(), viewportHeight, reservedBottom);
  }
  return true;
}

void DualDisplayAvatar::setExpression(m5avatar::Expression expression) {
  expression_ = expression;
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
  if (now - lastFrame_ < app::kFrameIntervalMs) return;
  lastFrame_ = now;
  if (now - lastGaze_ >= 1800) {
    gazeV_ = random(-100, 101) / 100.0f;
    gazeH_ = random(-100, 101) / 100.0f;
    lastGaze_ = now;
  }
  // Keep each blink visible for one complete frame, even at low HDMI FPS.
  float eyes = 1.0f;
  if (now - lastBlink_ >= blinkInterval_) {
    eyes = 0.0f;
    lastBlink_ = now;
    blinkInterval_ = random(2500, 5001);
  }
  const float breath = sinf((now % 4000) * (2.0f * PI / 4000.0f));
  const float mouth = mouthDemo_
                          ? fabsf(sinf((now % 600) * (2.0f * PI / 600.0f)))
                          : mouthOpenRatio_;
  const m5avatar::Gaze gaze(gazeV_, gazeH_);

  for (auto& target : targets_) {
    if (!target.ready) continue;
    // The library releases its face canvas at the end of every draw. Check
    // the next allocation before calling draw(), which doesn't report errors.
    if (!target.canvas->createSprite(target.size, target.size)) {
      Serial.println("ERROR: avatar canvas allocation failed; target disabled.");
      target.ready = false;
      continue;
    }
    m5avatar::DrawContext context(
        expression_, breath * target.scale, &palette_, gaze, eyes, gaze, eyes,
        mouth, "", 0.0f, 1.0f, 1, m5avatar::BatteryIconStatus::invisible, 0,
        nullptr);
    int32_t clipX, clipY, clipW, clipH;
    target.display->getClipRect(&clipX, &clipY, &clipW, &clipH);
    target.display->setClipRect(0, 0, target.display->width(), target.viewportHeight);
    target.face->draw(&context);
    target.display->setClipRect(clipX, clipY, clipW, clipH);
  }
}
