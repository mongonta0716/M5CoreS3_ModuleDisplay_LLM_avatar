#pragma once

#include <stdint.h>

// 1: native 1280x720, 2: 640x360 -> 720p, 4: 320x180 -> 720p.
// Scaling is performed by ModuleDisplay, keeping the HDMI signal at 720p.
#ifndef AVATAR_HDMI_SCALE
#define AVATAR_HDMI_SCALE 1
#endif

static_assert(AVATAR_HDMI_SCALE == 1 || AVATAR_HDMI_SCALE == 2 ||
                  AVATAR_HDMI_SCALE == 4,
              "AVATAR_HDMI_SCALE must be 1, 2, or 4");

namespace app {
constexpr uint8_t kDisplayScale = AVATAR_HDMI_SCALE;
constexpr uint16_t kLogicalWidth = 1280 / kDisplayScale;
constexpr uint16_t kLogicalHeight = 720 / kDisplayScale;
constexpr uint32_t kFrameIntervalMs = 66;
}  // namespace app
