#include <M5ModuleDisplay.h>

namespace {
M5ModuleDisplay display;
}

void setup() {
  Serial.begin(115200);
  Serial.println("\nTEST: ModuleDisplay Hello World (M5GFX only)");
  Serial.printf("Build: %s %s; free PSRAM=%u\n", __DATE__, __TIME__,
                ESP.getFreePsram());
  // ModuleDisplay is externally powered. Do not initialize CoreS3's LCD,
  // touch, PMIC, or other peripherals while testing this standalone display.
  if (!display.init()) {
    Serial.println("HelloWorld: HDMI initialization failed; text was not drawn.");
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
}

void loop() {
  delay(1000);
}
