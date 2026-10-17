#include <M5ModuleDisplay.h>
#include "AppConfig.h"
#ifdef HDMI_SOFTWARE_I2C
#include "CoreS3ModuleDisplay.h"
#endif
#ifdef HDMI_READBACK_DIAGNOSTICS
#include "HdmiDiagnostics.h"
#endif
#ifdef HELLO_THEN_AVATAR
#include "DualDisplayAvatar.h"
#include "HdmiChime.h"
#include "MicLipSync.h"
#endif

namespace {
#ifdef HDMI_SOFTWARE_I2C
CoreS3ModuleDisplay display(-1);
#else
M5ModuleDisplay display(app::kLogicalWidth, app::kLogicalHeight, 60,
                        1280, 720, app::kDisplayScale, app::kDisplayScale, 74250000);
#endif
#ifdef HELLO_THEN_AVATAR
DualDisplayAvatar avatar;
bool avatarReady = false;
constexpr uint32_t kHelloDurationMs = 2000;
HdmiChime chime;
MicLipSync lipSync;
bool chimeActive = false;
uint32_t chimeStartedAt = 0;
constexpr uint16_t kTouchButtonHeight = 40;

void playChime(const char* source) {
  // CoreS3 mic MCLK and HDMI LRCK share GPIO0 and I2S1; use them in turn.
  lipSync.pause();
  avatar.setMouthOpenRatio(0);
  chimeActive = chime.begin() && chime.play(source);
  chimeStartedAt = millis();
  if (!chimeActive) {
    chime.end();
    lipSync.begin();
  }
}

void updateAudio() {
  // PCM is 340 ms; allow DMA tail / HDMI output to finish before releasing I2S.
  if (chimeActive && millis() - chimeStartedAt >= 450 && !chime.isPlaying()) {
    chime.end();
    chimeActive = false;
    lipSync.begin();
  }
  lipSync.logStats(millis());
}

void drawTouchButtons() {
  auto& lcd = M5.Display;
  const int top = lcd.height() - kTouchButtonHeight;
  lcd.setTextDatum(middle_center);
  lcd.setTextSize(2);
  for (int i = 0; i < 3; ++i) {
    const int left = i * lcd.width() / 3;
    const int right = (i + 1) * lcd.width() / 3;
    const uint16_t color = i == 0 ? (M5.BtnA.isPressed() ? TFT_BLUE : TFT_NAVY)
                                   : TFT_DARKGREY;
    lcd.fillRect(left, top, right - left, kTouchButtonHeight, color);
    lcd.drawRect(left, top, right - left, kTouchButtonHeight, TFT_WHITE);
    lcd.setTextColor(TFT_WHITE, color);
    lcd.drawString(i == 0 ? "A: PiPo" : (i == 1 ? "B" : "C"),
                   (left + right) / 2, top + kTouchButtonHeight / 2);
  }
}

void initTouchButtons() {
  auto cfg = M5.config();
  cfg.internal_mic = true;  // Configure CoreS3 pins and ES7210 enable callback.
  cfg.internal_spk = false;
  cfg.internal_imu = false;
  cfg.internal_rtc = false;
  cfg.pmic_button = false;
  cfg.external_speaker_value = 0;
  cfg.external_display_value = 0;
  cfg.output_power = false;  // ModuleDisplay has its own external supply.
  M5.begin(cfg);
  M5.Display.setRotation(1);
  M5.Display.setBrightness(128);
  M5.setTouchButtonHeight(kTouchButtonHeight);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.drawString("HDMI Avatar", 160, 70);
  M5.Display.drawString("Tap A: Pi-Po", 160, 110);
  drawTouchButtons();
  Serial.printf("TouchButton: bottom %u px; touch=%s\n",
                M5.getTouchButtonHeight(), M5.Touch.isEnabled() ? "ready" : "unavailable");
}

void updateTouchButtons() {
  M5.update();
  if (M5.BtnA.wasPressed()) {
    playChime("BtnA");
    drawTouchButtons();
  } else if (M5.BtnA.wasReleased()) {
    drawTouchButtons();
  }
  updateAudio();
}

void startAvatar(bool hdmiAvailable) {
  const bool coreReady = avatar.addDisplay(M5.Display, kTouchButtonHeight);
  const bool hdmiReady = hdmiAvailable && avatar.addDisplay(display);
  avatarReady = coreReady || hdmiReady;
  Serial.printf("Avatar targets: CoreS3=%s HDMI=%s\n",
                coreReady ? "ready" : "unavailable", hdmiReady ? "ready" : "unavailable");
  if (avatarReady) {
    avatar.update(millis());
    Serial.println("Avatar: first frame rendered; animation running.");
    Serial.println("Serial: n/h/a/s/d/z (expression), t (mouth demo).");
  } else {
    Serial.println("ERROR: Avatar initialization failed.");
  }
}
#endif
}

void setup() {
  Serial.begin(115200);
#ifdef HELLO_THEN_AVATAR
  Serial.println("\nAPP: Hello World -> Avatar (CoreS3 + HDMI + mic FFT)");
#else
  Serial.println("\nTEST: ModuleDisplay Hello World (M5GFX only)");
#endif
  Serial.println("HDMI test: M5GFX 0.2.33 / transmitter ACK fix #339");
#ifdef HDMI_SOFTWARE_I2C
  Serial.println("HDMI test: software I2C port=-1 SDA=12 SCL=11");
#endif
  Serial.printf("Build: %s %s; free PSRAM=%u\n", __DATE__, __TIME__,
                ESP.getFreePsram());
#ifdef HELLO_THEN_AVATAR
  // Initialize CoreS3 touch/LCD before the separate software-I2C HDMI path.
  initTouchButtons();
#endif
  // ModuleDisplay is externally powered and initialized separately.
  if (!display.init()) {
    Serial.println("HelloWorld: HDMI initialization failed; text was not drawn.");
#ifdef HDMI_READBACK_DIAGNOSTICS
    diagnoseHdmiI2c();
#endif
#ifdef HELLO_THEN_AVATAR
    lipSync.begin();
    startAvatar(false);
#endif
    return;
  }

  Serial.printf("HDMI: output 1280x720@60, logical %dx%d, hardware scale %u\n",
                display.width(), display.height(), app::kDisplayScale);
  display.fillScreen(TFT_NAVY);
  display.drawRect(0, 0, display.width(), display.height(), TFT_WHITE);
  display.setTextColor(TFT_WHITE, TFT_NAVY);
  display.setTextSize(6.0f / app::kDisplayScale);
  display.setTextDatum(middle_center);
  display.drawString("Hello World", display.width() / 2, display.height() / 2);
  Serial.printf("HelloWorld: text drawn on HDMI %dx%d\n",
                display.width(), display.height());
#ifdef HELLO_THEN_AVATAR
  const uint32_t helloShownAt = millis();
  playChime("startup");
  while (millis() - helloShownAt < kHelloDurationMs) {
    updateTouchButtons();
    delay(1);
  }
  Serial.printf("HelloWorld: held for %u ms; starting Avatar\n",
                static_cast<unsigned>(millis() - helloShownAt));
  startAvatar(true);
#endif
}

void loop() {
#ifdef HELLO_THEN_AVATAR
  updateTouchButtons();
  if (avatarReady) {
    while (Serial.available()) {
      avatar.handleCommand(static_cast<char>(Serial.read()));
    }
    avatar.setMouthOpenRatio(lipSync.mouthRatio());
    avatar.update(millis());
  }
  delay(1);
#else
  delay(1000);
#endif
}
