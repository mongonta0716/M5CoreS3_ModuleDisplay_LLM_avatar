#include <M5ModuleDisplay.h>
#ifdef HDMI_SOFTWARE_I2C
#include "CoreS3ModuleDisplay.h"
#endif
#ifdef HDMI_READBACK_DIAGNOSTICS
#include "HdmiDiagnostics.h"
#endif
#ifdef HELLO_THEN_AVATAR
#include "DualDisplayAvatar.h"
#endif

namespace {
#ifdef HDMI_SOFTWARE_I2C
CoreS3ModuleDisplay display(-1);
#else
M5ModuleDisplay display;
#endif
#ifdef HELLO_THEN_AVATAR
DualDisplayAvatar avatar;
bool avatarReady = false;
constexpr uint32_t kHelloDurationMs = 2000;
#endif
}

void setup() {
  Serial.begin(115200);
#ifdef HELLO_THEN_AVATAR
  Serial.println("\nAPP: ModuleDisplay Hello World -> Avatar (HDMI only)");
#else
  Serial.println("\nTEST: ModuleDisplay Hello World (M5GFX only)");
#endif
  Serial.println("HDMI test: M5GFX 0.2.33 / transmitter ACK fix #339");
#ifdef HDMI_SOFTWARE_I2C
  Serial.println("HDMI test: software I2C port=-1 SDA=12 SCL=11");
#endif
  Serial.printf("Build: %s %s; free PSRAM=%u\n", __DATE__, __TIME__,
                ESP.getFreePsram());
  // ModuleDisplay is externally powered. Do not initialize CoreS3's LCD,
  // touch, PMIC, or other peripherals while testing this standalone display.
  if (!display.init()) {
    Serial.println("HelloWorld: HDMI initialization failed; text was not drawn.");
#ifdef HDMI_READBACK_DIAGNOSTICS
    diagnoseHdmiI2c();
#endif
    return;
  }

  display.fillScreen(TFT_NAVY);
  display.drawRect(0, 0, display.width(), display.height(), TFT_WHITE);
  display.setTextColor(TFT_WHITE, TFT_NAVY);
  display.setTextSize(6);
  display.setTextDatum(middle_center);
  display.drawString("Hello World", display.width() / 2, display.height() / 2);
  Serial.printf("HelloWorld: text drawn on HDMI %dx%d\n",
                display.width(), display.height());
#ifdef HELLO_THEN_AVATAR
  const uint32_t helloShownAt = millis();
  while (millis() - helloShownAt < kHelloDurationMs) {
    delay(1);
  }
  Serial.printf("HelloWorld: held for %u ms; starting Avatar\n",
                static_cast<unsigned>(millis() - helloShownAt));
  avatarReady = avatar.addDisplay(display);
  if (avatarReady) {
    avatar.update(millis());
    Serial.println("Avatar: first frame rendered; animation running.");
    Serial.println("Serial: n/h/a/s/d/z (expression), t (mouth demo).");
  } else {
    Serial.println("ERROR: Avatar initialization failed.");
  }
#endif
}

void loop() {
#ifdef HELLO_THEN_AVATAR
  if (avatarReady) {
    while (Serial.available()) {
      avatar.handleCommand(static_cast<char>(Serial.read()));
    }
    avatar.update(millis());
  }
  delay(1);
#else
  delay(1000);
#endif
}
