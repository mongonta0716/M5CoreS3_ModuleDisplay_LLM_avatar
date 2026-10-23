#include <M5Unified.h>
#include <esp_heap_caps.h>
#include <memory>
#include <new>
#include "AppConfig.h"
#include "AtomAudio.h"
#include "AtomDisplaySettings.h"
#include "CalibrationButton.h"
#include "CalibrationText.h"
#include "DualDisplayAvatar.h"
#include "MicLipSync.h"
#include "VowelCalibrationStore.h"
#include "Chime.h"

#if !APP_ATOMS3
#error "Select env:AtomS3 for this application"
#endif

namespace {
#if defined(ARDUINO_M5STACK_ATOMS3R)
constexpr const char* kBoardName = "AtomS3R";
constexpr auto kFallbackBoard = m5::board_t::board_M5AtomS3R;
#else
constexpr const char* kBoardName = "AtomS3";
constexpr auto kFallbackBoard = m5::board_t::board_M5AtomS3;
#endif

void logMemory(const char* stage) {
  constexpr uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
  Serial.printf("Atom memory: %s internal free=%u min=%u largest=%u PSRAM=%u\n",
      stage, unsigned(heap_caps_get_free_size(caps)),
      unsigned(heap_caps_get_minimum_free_size(caps)),
      unsigned(heap_caps_get_largest_free_block(caps)), unsigned(ESP.getPsramSize()));
}

std::unique_ptr<DualDisplayAvatar> avatar;
MicLipSync lipSync;
VowelCalibration calibration;
CalibrationButton button;
Chime chime;
atomAudio::Input input = atomAudio::Input::PdmUnit;
bool avatarReady = false, micReady = false;
uint32_t lastUiAt = 0;
uint8_t screenRotation = 0;

bool createAvatar(m5avatar::Expression expression = m5avatar::Expression::Neutral,
                  bool mouthDemo = false) {
  // Recreate after rotation as required by the rendering library. Free the
  // old target first so repeated rotations do not retain canvases or DMA strips.
  avatar.reset();
  avatar.reset(new (std::nothrow) DualDisplayAvatar);
  if (!avatar || !avatar->addDisplay(M5.Display)) return false;
  if (expression != m5avatar::Expression::Neutral) avatar->setExpression(expression);
  if (mouthDemo) avatar->handleCommand('t');
  return true;
}

void rotateDisplay() {
  const auto expression = avatar ? avatar->getExpression() : m5avatar::Expression::Neutral;
  const bool mouthDemo = avatar && avatar->isMouthDemoEnabled();
  const uint8_t previous = screenRotation;
  const uint8_t next = (previous + 1) % 4;
  M5.Display.setRotation(next);
  M5.Display.fillScreen(TFT_BLACK);
  avatarReady = createAvatar(expression, mouthDemo);
  if (avatarReady) {
    screenRotation = next;
    const bool saved = atomDisplay::saveRotation(screenRotation);
    Serial.printf("Atom rotation: %u (%u deg) NVS=%s\n", screenRotation,
                  screenRotation * 90, saved ? "OK" : "FAIL");
  } else {
    M5.Display.setRotation(previous);
    avatarReady = createAvatar(expression, mouthDemo);
    Serial.println("ERROR: rotation failed; restored previous orientation");
  }
  lastUiAt = millis() - 100;  // Refresh any active calibration prompt immediately.
  logMemory("rotation");
}

void startCalibration() {
  if (!micReady) { Serial.println("Atom calibration: microphone unavailable"); return; }
  calibration.start(millis());
  logMemory("calibration-start");
  Serial.println("Atom calibration: quiet 2 s, then each vowel 3 s twice with 2 s rests");
}
void resetCalibration() {
  const bool saved = vowelStorage::reset(atomAudio::profileKey(input));
  if (saved) lipSync.setVowelModel(VowelModel{});
  calibration.setStatus(saved ? VowelCalibration::Status::Reset :
                               VowelCalibration::Status::SaveFailed, millis());
  Serial.printf("Atom calibration: current input reset %s\n", saved ? "OK" : "FAIL");
  logMemory("calibration-reset");
}
void updateCalibration(uint32_t now) {
  if (calibration.update(now, lipSync.vowelFeatures())) {
    const bool saved = vowelStorage::save(calibration.model(), atomAudio::profileKey(input));
    lipSync.setVowelModel(calibration.model());
    if (!saved) calibration.setStatus(VowelCalibration::Status::SaveFailed, now);
    Serial.printf("Atom calibration: save %s\n", saved ? "OK" : "FAIL");
    logMemory("calibration-save");
  }
}
void drawMessage(const String& title, const String& detail) {
  // A dedicated calibration screen keeps two lines readable on 128x128.
  auto& lcd = M5.Display;
  lcd.fillScreen(TFT_BLACK);
  lcd.setFont(&fonts::efontJA_16);
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  lcd.setTextSize(1.25f);
  lcd.drawString(title, lcd.width() / 2, 40);
  lcd.setTextSize(1);
  lcd.drawString(detail, lcd.width() / 2, 75);
}
void render(uint32_t now) {
  if (!avatarReady || !micReady) return;
  String title, detail;
  calibrationText(calibration, now, "Aで再調整", title, detail);
  if (title.length()) {
    if (now - lastUiAt >= 100) { drawMessage(title, detail); lastUiAt = now; }
    return;
  }
  const auto features = lipSync.vowelFeatures();
  avatar->setMouthOpenRatio(lipSync.mouthRatio());
  avatar->setVowel(features.vowel, features.confidence);
  avatar->update(now);
}
}

void setup() {
  Serial.begin(115200);
  Serial.printf("\nAPP: %s Avatar; automatic EchoBase/PDM; A=Cal, double=Rotate, hold 3s=Reset\n", kBoardName);
  input = atomAudio::detect();
  auto cfg = M5.config();
  cfg.internal_mic = false;
  cfg.internal_spk = false;
  cfg.internal_imu = false;
  cfg.internal_rtc = false;
  cfg.pmic_button = false;
  cfg.external_display_value = 0;
  cfg.external_speaker_value = 0;
  cfg.external_speaker.atomic_echo = input == atomAudio::Input::EchoBase;
  cfg.fallback_board = kFallbackBoard;
  M5.begin(cfg);
  logMemory("M5.begin");
  screenRotation = atomDisplay::loadRotation();
  M5.Display.setRotation(screenRotation);
  Serial.printf("Atom rotation: boot=%u (%u deg)\n", screenRotation, screenRotation * 90);
  M5.Display.setBrightness(128);
  atomAudio::configureCapture(input);
  Serial.printf("Atom board: type=%d LCD=%dx%d free PSRAM=%u\n",
                int(M5.getBoard()), M5.Display.width(), M5.Display.height(), ESP.getFreePsram());
  drawMessage("Hello World", atomAudio::name(input));
  const uint32_t helloAt = millis();
  if (input == atomAudio::Input::EchoBase) {
    const bool playing = chime.begin() && chime.play("startup");
    const uint32_t started = millis();
    while (playing && (millis() - started < 450 || chime.isPlaying()) &&
           millis() - started < 1500) delay(1);
    chime.end();  // Stop TX before the microphone owns the shared I2S port.
  }
  while (millis() - helloAt < 2000) delay(1);
  lipSync.setVowelModel(vowelStorage::load(atomAudio::profileKey(input)));
  micReady = lipSync.begin();
  avatarReady = createAvatar();
  if (!micReady || !avatarReady) drawMessage("初期化失敗", "接続して再起動");
  Serial.printf("Atom ready: audio=%s mic=%d avatar=%d; profile=%s\n",
      atomAudio::name(input), micReady, avatarReady, atomAudio::profileKey(input));
  logMemory("ready");
}

void loop() {
  M5.update();
  const uint32_t now = millis();
  const auto action = button.update(M5.BtnA.isPressed(), now);
  if (action == CalibrationButton::Action::Reset) resetCalibration();
  else if (action == CalibrationButton::Action::Calibrate) startCalibration();
  else if (action == CalibrationButton::Action::Rotate) rotateDisplay();
  while (Serial.available()) {
    const char command = static_cast<char>(Serial.read());
    if (command == 'c') startCalibration();
    else if (command == 'x') resetCalibration();
    else if (avatar) avatar->handleCommand(command);
  }
  updateCalibration(now);
  render(now);
  lipSync.logStats(now);
  delay(1);
}
