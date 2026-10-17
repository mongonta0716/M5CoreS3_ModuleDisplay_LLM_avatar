// Include this before M5Unified so its configuration includes ModuleDisplay.
#include <M5ModuleDisplay.h>
#include <M5Unified.h>

#include "AppConfig.h"
#include "DualDisplayAvatar.h"
#include "HdmiDiagnostics.h"

namespace {
DualDisplayAvatar avatar;

bool initModuleDisplay() {
  M5ModuleDisplay::config_t cfg;
  cfg.logical_width = app::kLogicalWidth;
  cfg.logical_height = app::kLogicalHeight;
  cfg.output_width = 1280;
  cfg.output_height = 720;
  cfg.refresh_rate = 60;
  cfg.scale_w = app::kDisplayScale;
  cfg.scale_h = app::kDisplayScale;
  cfg.pixel_clock = 74250000;

  // Allow the stacked modules' power rails to settle before talking to HDMI.
  delay(500);
  for (unsigned attempt = 1; attempt <= 3; ++attempt) {
    Serial.printf("ModuleDisplay init attempt %u/3\n", attempt);
    // Use a fresh object: init() on a failed M5ModuleDisplay can otherwise
    // return true simply because its panel object has already been allocated.
    M5ModuleDisplay display(cfg);
    if (display.init()) {
      M5.addDisplay(display);
      return true;
    }
    if (attempt < 3) delay(500);
  }
  Serial.println("ERROR: ModuleDisplay initialization failed. "
                 "Check the M5HDMI Chip ID / FPGA ID / I2C errors above.");
  diagnoseHdmiI2c();
  return false;
}
}

void setup() {
  auto cfg = M5.config();
  cfg.output_power = true;
  cfg.internal_mic = false;
  cfg.internal_spk = false;
  cfg.external_speaker_value = 0;
  cfg.external_display_value = 0;
  // Initialize HDMI separately, after power setup, with bounded retries.
  M5.begin(cfg);
  Serial.begin(115200);
  Serial.printf("Power: M-Bus output %s, USB %d mV, TS %.3f V\n",
                M5.Power.getExtOutput() ? "enabled" : "disabled",
                M5.Power.getVBUSVoltage(), M5.Power.Axp2101.getTSVoltage());
  if (!M5.Power.getExtOutput()) {
    Serial.println("NOTE: M-Bus output disabled; this can occur with external "
                   "stack power and does not alone prove a power fault.");
  }
  if (ESP.getPsramSize() == 0) {
    Serial.println("ERROR: PSRAM unavailable. CoreS3 requires qio_qspi; "
                   "rebuild and flash the bootloader and application.");
  }
  initModuleDisplay();

  const int coreIndex = M5.getDisplayIndex(m5::board_t::board_M5StackCoreS3);
  const int moduleIndex = M5.getDisplayIndex(m5::board_t::board_M5ModuleDisplay);
  Serial.printf("\nCoreS3 + ModuleLLM + ModuleDisplay avatar\n"
                "Displays: %u, internal: %d, HDMI: %d\n"
                "Free heap: %u, free PSRAM: %u\n",
                static_cast<unsigned>(M5.getDisplayCount()), coreIndex,
                moduleIndex, ESP.getFreeHeap(), ESP.getFreePsram());

  if (coreIndex >= 0) {
    auto& display = M5.Displays(coreIndex);
    display.setRotation(1);
    display.setBrightness(128);
    avatar.addDisplay(display);
  } else {
    Serial.println("ERROR: CoreS3 display not found.");
  }

  if (moduleIndex >= 0) {
    auto& display = M5.Displays(moduleIndex);
    // ModuleDisplay already initializes in landscape. Preserve its rotation.
    Serial.printf("HDMI: output 1280x720@60, logical %dx%d, scale %u\n",
                  display.width(), display.height(), app::kDisplayScale);
    if (display.width() == app::kLogicalWidth &&
        display.height() == app::kLogicalHeight) {
      uint8_t scale = app::kDisplayScale;
      while (!avatar.addDisplay(display) && scale < 4) {
        scale *= 2;
        Serial.printf("Retry HDMI avatar at %dx%d, hardware scale %u\n",
                      1280 / scale, 720 / scale, scale);
        if (!display.setResolution(1280 / scale, 720 / scale, 60,
                                   1280, 720, scale, scale, 74250000)) {
          Serial.println("ERROR: could not change HDMI logical resolution.");
          break;
        }
      }
    } else {
      Serial.println("ERROR: unexpected HDMI logical resolution.");
    }
  } else {
    Serial.println("WARNING: ModuleDisplay unavailable (missing or init failed); "
                   "using CoreS3 only.");
  }
  Serial.println("Touch CoreS3: next expression. Serial: n/h/a/s/d/z, t (mouth demo).");
}

void loop() {
  M5.update();
  if (M5.Touch.getDetail().wasPressed()) {
    avatar.nextExpression();
  }
  while (Serial.available()) {
    avatar.handleCommand(static_cast<char>(Serial.read()));
  }
  avatar.update(millis());
  delay(1);
}
