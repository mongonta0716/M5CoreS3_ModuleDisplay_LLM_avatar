#pragma once

#include <stdint.h>

// 0: CoreS3 microphone and local DSP; 1: ModuleLLM microphone/DSP over UART.
// A build flag (-DUSE_MODULELLM=0 or 1) overrides this default.
#ifndef USE_MODULELLM
#define USE_MODULELLM 0
#endif

#if USE_MODULELLM != 0 && USE_MODULELLM != 1
#error "USE_MODULELLM must be 0 or 1"
#endif

// 0: CoreS3 LCD/speaker only; 1: add ModuleDisplay HDMI video/audio.
#ifndef USE_MODULEDISPLAY
#define USE_MODULEDISPLAY 0
#endif

#if USE_MODULEDISPLAY != 0 && USE_MODULEDISPLAY != 1
#error "USE_MODULEDISPLAY must be 0 or 1"
#endif

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
