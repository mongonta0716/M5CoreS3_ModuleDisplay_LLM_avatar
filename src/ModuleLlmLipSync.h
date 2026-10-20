#pragma once

#include <M5Unified.h>
#include "VowelWire.h"
#include "VowelCalibration.h"

#ifndef MODULELLM_UART_RX
#define MODULELLM_UART_RX 18
#endif
#ifndef MODULELLM_UART_TX
#define MODULELLM_UART_TX 17
#endif

class RemoteCalibration {
 public:
  using Status = VowelCalibration::Status;
  void update(const vowelWire::Frame& frame, uint32_t now) { frame_ = frame; arrived_ = now; valid_ = true; }
  void clear() { valid_ = false; }
  Status status(uint32_t now) const {
    return valid_ && now - arrived_ < vowelWire::kTimeoutMs ? Status(frame_.status) : Status::Idle;
  }
  bool active() const { return status(millis()) == Status::Running; }
  unsigned step() const { return frame_.step; }
  unsigned round() const { return frame_.round; }
  bool resting() const { return frame_.rest != 0; }
  unsigned secondsLeft(uint32_t) const { return frame_.seconds; }
 private:
  vowelWire::Frame frame_;
  uint32_t arrived_ = 0;
  bool valid_ = false;
};

// All UART work happens on the main task. No CoreS3 microphone/FFT is started.
class ModuleLlmLipSync {
 public:
  bool begin() {
    if (!started_) {
      Serial2.setRxBufferSize(4096);
      Serial2.setTxBufferSize(256);
      Serial2.begin(115200, SERIAL_8N1, MODULELLM_UART_RX, MODULELLM_UART_TX);
      started_ = true;
      reconnect();
      Serial.printf("ModuleLLM UART: 115200 8N1 RX=%d TX=%d; CoreS3 mic disabled\n", MODULELLM_UART_RX, MODULELLM_UART_TX);
    }
    muted_ = false; command(vowelWire::Command::Resume); return true;
  }
  void pause() { muted_ = true; command(vowelWire::Command::Pause); }
  void startCalibration() { command(vowelWire::Command::Calibrate); }
  void resetCalibration() { command(vowelWire::Command::Reset); }
  const RemoteCalibration& calibration() const { return calibration_; }
  float mouthRatio() const {
    return !muted_ && receiver_.fresh(millis()) ? receiver_.frame().open : 0;
  }
  VowelFeatures vowelFeatures() const {
    VowelFeatures f;
    if (!muted_ && receiver_.fresh(millis())) {
      const auto& frame = receiver_.frame();
      f.sequence = frame.sequence; f.vowel = vowelWire::vowel(frame.text); f.confidence = frame.confidence;
    }
    return f;
  }
  void logStats(uint32_t now) {
    update(now);
    if (now - lastLog_ < 1000) return;
    lastLog_ = now;
    Serial.printf("ModuleLLM: %s accepted=%u invalid=%u stale=%u text=%s mouth=%.2f\n",
                  receiver_.fresh(now) ? "connected" : "waiting", accepted_, invalid_, stale_,
                  receiver_.fresh(now) ? receiver_.frame().text.c_str() : "無音", mouthRatio());
  }
 private:
  vowelWire::Lines lines_;
  vowelWire::Receiver receiver_;
  vowelWire::Control control_;
  RemoteCalibration calibration_;
  bool started_ = false, muted_ = true, pending_ = false;
  uint32_t lastSent_ = 0, lastGood_ = 0, lastLog_ = 0;
  unsigned accepted_ = 0, invalid_ = 0, stale_ = 0;
  void reconnect() {
    control_.client = esp_random(); if (!control_.client) control_.client = 1;
    control_.epoch = 1; lines_.reset(); calibration_.clear();
    receiver_.expect(control_.client, control_.epoch); pending_ = true;
    lastGood_ = millis();
  }
  void command(vowelWire::Command c) {
    control_.command = c;
    if (++control_.epoch == 0) reconnect();
    receiver_.expect(control_.client, control_.epoch); calibration_.clear(); pending_ = true;
    lastSent_ = millis() - 200;
  }
  void update(uint32_t now) {
    if (!started_) return;
    unsigned budget = 2048;
    while (budget-- && Serial2.available()) {
      std::string line;
      if (!lines_.push(char(Serial2.read()), line)) continue;
      vowelWire::Frame frame;
      if (!vowelWire::decode(line, frame)) { ++invalid_; continue; }
      if (!receiver_.accept(frame, now)) { ++stale_; continue; }
      ++accepted_; lastGood_ = now; pending_ = false; calibration_.update(frame, now);
    }
    // A new client nonce discards old sessions and lets a restarted server
    // resynchronize without ever accepting a different boot's queued results.
    if (now - lastGood_ >= 1000) {
      const auto wanted = muted_ ? vowelWire::Command::Pause : vowelWire::Command::Resume;
      reconnect(); control_.command = wanted; lastSent_ = now - 200;
    }
    if (pending_ && now - lastSent_ >= 200) {
      const auto line = vowelWire::encode(control_);
      if (Serial2.availableForWrite() >= int(line.size())) {
        Serial2.write(reinterpret_cast<const uint8_t*>(line.data()), line.size()); lastSent_ = now;
      }
    }
  }
};
