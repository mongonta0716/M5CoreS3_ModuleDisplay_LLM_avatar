#pragma once

#include <Drawable.h>
#include <DrawContext.h>
#include "VowelModel.h"

class VowelMouth : public m5avatar::Drawable {
 public:
  VowelMouth(float scale, const MouthPose* pose, const String* title, const String* detail)
      : scale_(scale), pose_(pose), title_(title), detail_(detail) {}
  void draw(M5Canvas* canvas, m5avatar::BoundingRect rect,
            m5avatar::DrawContext* ctx) override {
    const int w = std::max(1L, lroundf(pose_->width * scale_));
    const int h = std::max(1L, lroundf(pose_->height * scale_));
    const int radius = lroundf(std::min(w, h) * 0.5f * pose_->roundness);
    const int x = rect.getLeft() - w / 2;
    const int y = rect.getTop() - h / 2 + std::min(1.0f, ctx->getBreath()) * 2;
    const uint32_t color = ctx->getColorDepth() == 1 ? 1 :
        ctx->getColorPalette()->get(COLOR_PRIMARY);
    canvas->fillRoundRect(x, y, w, h, radius, color);
    drawBalloon(canvas, rect, ctx);
  }
 private:
  float scale_;
  const MouthPose* pose_;
  const String* title_;
  const String* detail_;

  void drawBalloon(M5Canvas* canvas, m5avatar::BoundingRect rect,
                   m5avatar::DrawContext* ctx) {
    // Source coordinates are relative to the mouth, so the balloon scales and
    // clips with the face on both displays. Avoid upstream Balloon's fixed
    // coordinates and its modifications to the global M5.Lcd text settings.
    auto scaled = [this](float value) { return lroundf(value * scale_); };
    const int left = rect.getLeft() + scaled(80);
    const int top = rect.getTop() + scaled(5);
    const int width = scaled(60), height = scaled(44);
    const uint32_t foreground = ctx->getColorDepth() == 1 ? 1 :
        ctx->getColorPalette()->get(COLOR_PRIMARY);
    const uint32_t background = ctx->getColorDepth() == 1 ? 0 :
        ctx->getColorPalette()->get(COLOR_BACKGROUND);
    if (title_->length()) {
      const int messageLeft = rect.getLeft() - scaled(33);
      const int messageTop = rect.getTop() - scaled(145);
      const int messageWidth = scaled(180), messageHeight = scaled(74);
      canvas->fillTriangle(messageLeft + scaled(20), messageTop + scaled(52),
                           messageLeft + scaled(17), messageTop + scaled(75),
                           messageLeft + scaled(40), messageTop + scaled(52), foreground);
      canvas->fillRoundRect(messageLeft, messageTop, messageWidth, messageHeight, scaled(10), foreground);
      canvas->setFont(&fonts::efontJA_16);
      canvas->setTextSize(1.5f);  // 24 px on either canvas; never shrink below 1.
      canvas->setTextDatum(middle_center);
      canvas->setTextColor(background, foreground);
      canvas->drawString(*title_, messageLeft + messageWidth / 2, messageTop + scaled(20));
      canvas->drawString(*detail_, messageLeft + messageWidth / 2, messageTop + scaled(52));
      return;
    }
    canvas->fillTriangle(left + scaled(4), top + scaled(12),
                         left - scaled(16), top + scaled(4),
                         left + scaled(4), top + scaled(28), foreground);
    canvas->fillRoundRect(left, top, width, height, scaled(10), foreground);
    canvas->setFont(&fonts::efontJA_16);
    canvas->setTextSize(2.0f * scale_);
    canvas->setTextDatum(middle_center);
    canvas->setTextColor(background, foreground);
    canvas->drawString(vowelKana(pose_->label), left + width / 2, top + height / 2);
  }
};
