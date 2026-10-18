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
#include "VowelCalibration.h"
#include "VowelCalibrationStore.h"
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
VowelCalibration calibration;
bool chimeActive = false;
uint32_t chimeStartedAt = 0;
constexpr uint16_t kTouchButtonHeight = 40;

void playChime(const char* source) {
  calibration.cancel(millis());
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

void startCalibration() {
  if (!avatarReady || chimeActive) return;
  calibration.start(millis());
  Serial.println("Vowel calibration: quiet 2 s, then A/I/U/E/O for 3 s twice each, with 2 s breath breaks; A cancels.");
}

void resetCalibration() {
  const bool saved = vowelStorage::reset();
  lipSync.setVowelModel(VowelModel{});
  calibration.setStatus(saved ? VowelCalibration::Status::Reset :
                               VowelCalibration::Status::SaveFailed, millis());
  Serial.printf("Vowel calibration: reset %s\n", saved ? "complete" : "NVS failed");
}

void updateCalibration(const VowelFeatures& features) {
  const auto previousStep = calibration.step();
  const auto previousRound = calibration.round();
  const bool wasResting = calibration.resting();
  const bool wasActive = calibration.active();
  if (calibration.update(millis(), features)) {
    const bool saved = vowelStorage::save(calibration.model());
    lipSync.setVowelModel(calibration.model());
    if (!saved) calibration.setStatus(VowelCalibration::Status::SaveFailed, millis());
    Serial.printf("Vowel calibration: %s; noise RMS=%.0f\n",
                  saved ? "saved to NVS" : "applied, NVS save failed", calibration.model().noiseRms);
    for (unsigned i = 0; i < 5; ++i)
      Serial.printf("  %s: F1=%.0f F2=%.0f\n", vowelName(Vowel(i + 1)),
                    calibration.model().f1[i], calibration.model().f2[i]);
  } else if (calibration.active() && (calibration.step() != previousStep ||
             calibration.round() != previousRound || calibration.resting() != wasResting)) {
    if (previousStep == 0)
      Serial.printf("Vowel calibration: quiet median RMS=%.1f\n", calibration.model().noiseRms);
    if (previousStep > 0 && calibration.step() != previousStep)
      Serial.printf("Vowel calibration: measured %s F1=%.0f F2=%.0f\n", vowelName(Vowel(previousStep)),
                    calibration.model().f1[previousStep - 1], calibration.model().f2[previousStep - 1]);
    if (calibration.resting()) Serial.println("Vowel calibration: breathe for 2 s");
    else Serial.printf("Vowel calibration: hold %s for 3 s (%u/2)\n",
                       vowelName(calibration.expected()), calibration.round());
  } else if (wasActive && !calibration.active()) {
    Serial.printf("Vowel calibration: failed at step %u, samples=%u, status=%s; keeping previous model\n",
                  previousStep, calibration.sampleCount(),
                  calibration.status(millis()) == VowelCalibration::Status::Insufficient ? "insufficient" : "unstable");
  }
}

void updateCalibrationBalloon() {
  const auto status = calibration.status(millis());
  String title, detail;
  if (calibration.active()) {
    static const char* labels[] = {"静かに", "あー", "いー", "うー", "えー", "おー"};
    if (calibration.resting()) {
      title = "息つぎ";
      detail = String("あと") + calibration.secondsLeft(millis()) + "秒";
    } else if (calibration.step() == 0) {
      title = "声の調整";
      detail = String("静かに ") + calibration.secondsLeft(millis()) + "秒";
    } else {
      title = String(labels[calibration.step()]) + " " + calibration.round() + "回目";
      detail = String("あと") + calibration.secondsLeft(millis()) + "秒";
    }
  } else {
    switch (status) {
      case VowelCalibration::Status::Complete: title = "声の調整"; detail = "保存完了"; break;
      case VowelCalibration::Status::Insufficient: title = "音声不足"; detail = "Bで再調整"; break;
      case VowelCalibration::Status::Invalid: title = "母音不安定"; detail = "Bで再調整"; break;
      case VowelCalibration::Status::Cancelled: title = "声の調整"; detail = "中止"; break;
      case VowelCalibration::Status::Reset: title = "声の調整"; detail = "初期値です"; break;
      case VowelCalibration::Status::SaveFailed: title = "保存に失敗"; detail = "Bで再調整"; break;
      default: break;
    }
  }
  avatar.setBalloon(title, detail);
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
    lcd.drawString(i == 0 ? "A: PiPo" : (i == 1 ? "B: Cal" : "C: Reset"),
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
  M5.setTouchButtonHeight(40);
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
  static bool touching = false;
  const bool pressed = M5.Touch.getCount() && M5.Touch.getDetail().isPressed();
  if (pressed != touching) {
    touching = pressed;
    const auto point = M5.Touch.getDetail();
    Serial.printf("Touch: %s x=%d y=%d buttons=%d/%d/%d\n", pressed ? "down" : "up",
                  point.x, point.y, M5.BtnA.isPressed(), M5.BtnB.isPressed(), M5.BtnC.isPressed());
  }
  if (M5.BtnA.wasPressed()) {
    playChime("BtnA");
    drawTouchButtons();
  } else if (M5.BtnA.wasReleased()) {
    drawTouchButtons();
  }
  if (M5.BtnB.wasPressed()) startCalibration();
  if (M5.BtnC.wasPressed()) resetCalibration();
  updateAudio();
}

bool logTouchHealth(const char* stage = "runtime") {
  auto* touch = M5.Display.touch();
  if (!touch) { Serial.println("Touch health: no controller"); return false; }
  const auto cfg = touch->config();
  uint8_t data[6] = {};
  const bool ok = M5.In_I2C.readRegister(cfg.i2c_addr, 0xA3, data, sizeof(data), 100000);
  Serial.printf("Touch health (%s): enabled=%d board=%d port=%d addr=%02x IRQ=%d level=%d I2C=%s chip=%02x firmware=%02x vendor=%02x\n",
                stage, M5.Touch.isEnabled(), int(M5.getBoard()), cfg.i2c_port, cfg.i2c_addr,
                cfg.pin_int, cfg.pin_int >= 0 ? digitalRead(cfg.pin_int) : -1,
                ok ? "OK" : "FAIL", data[0], data[3], data[5]);
  return ok && data[5] != 0;
}

void recoverTouch() {
  // CoreS3 TOUCH_RST is AW9523 P0_0. An ESP reset does not pulse it.
  // Preserve the other outputs, including the M-Bus power and amplifier reset.
  uint8_t id = 0, outputs = 0;
  if (!M5.In_I2C.readRegister(0x58, 0x10, &id, 1, 100000) || id != 0x23 ||
      !M5.In_I2C.readRegister(0x58, 0x02, &outputs, 1, 100000)) {
    Serial.println("Touch recovery: IO expander read failed");
    return;
  }
  const bool low = M5.In_I2C.writeRegister8(0x58, 0x02, outputs & ~uint8_t(1), 100000);
  delay(20);
  const bool high = M5.In_I2C.writeRegister8(0x58, 0x02, outputs | uint8_t(1), 100000);
  delay(200);
  const bool ready = M5.Display.touch()->init();
  Serial.printf("Touch recovery: reset low=%d high=%d init=%s\n", low, high, ready ? "OK" : "FAIL");
  logTouchHealth("after touch reset");
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
    logTouchHealth();
    Serial.println("Serial: n/h/a/s/d/z (expression), t (mouth demo), c (vowel calibration), x (reset calibration).");
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
  if (!logTouchHealth("before HDMI")) recoverTouch();
  lipSync.setVowelModel(vowelStorage::load());
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

#ifdef HELLO_THEN_AVATAR
  logTouchHealth("after HDMI");
#endif
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
      const char command = static_cast<char>(Serial.read());
      if (command == 'c') startCalibration();
      else if (command == 'x') resetCalibration();
      else if (command == 'v') logTouchHealth();
      else avatar.handleCommand(command);
    }
    const auto features = lipSync.vowelFeatures();
    updateCalibration(features);
    avatar.setMouthOpenRatio(lipSync.mouthRatio());
    avatar.setVowel(features.vowel, features.confidence);
    updateCalibrationBalloon();
    avatar.update(millis());
  }
  delay(1);
#else
  delay(1000);
#endif
}
