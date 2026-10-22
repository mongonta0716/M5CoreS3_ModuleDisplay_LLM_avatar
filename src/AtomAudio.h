#pragma once
#include <M5Unified.h>
#include <Wire.h>
#include "AtomAudioProfile.h"

namespace atomAudio {
// Based on the EchoBase-first selection in washishi's reference project.
// The PDM unit has no I2C identity: it is selected when ES8311 does not ACK.
inline Input detect() {
  bool responded = false;
  if (Wire.begin(38, 39, 100000)) {
    Wire.setTimeOut(50);
    for (unsigned attempt = 0; attempt < 3; ++attempt) {
      Wire.beginTransmission(0x18);
      if (Wire.endTransmission() == 0) { responded = true; break; }
      delay(10);
    }
    Wire.end();  // Leave M5Unified in control of audio I2C after probing.
  }
  const Input input = select(responded);
  Serial.printf("Atom audio: %s selected%s\n", name(input),
      responded ? " (ES8311 ACK)" : " (EchoBase absent; PDM presence unverified)");
  return input;
}

inline void configureCapture(Input input) {
  auto cfg = M5.Mic.config();
  cfg.sample_rate = 16000;
  cfg.over_sampling = 1;
  if (input == Input::EchoBase) {
    // ES8311 defaults to the left slot. Capture that slot, and duplicate
    // the mono startup sound across TX slots rather than TX right only.
    cfg.left_channel = true;
    auto speaker = M5.Speaker.config();
    speaker.stereo = true;
    M5.Speaker.config(speaker);
    Serial.println("Atom audio: ES8311 RX=left, TX=stereo");
  } else {
    cfg.i2s_port = I2S_NUM_0;  // ESP32-S3 PDM RX.
    cfg.pin_mck = -1;
    cfg.pin_bck = -1;         // Negative BCK selects M5Unified's PDM path.
    cfg.pin_ws = 1;          // Grove white = CLK.
    cfg.pin_data_in = 2;     // Grove yellow = DATA.
    cfg.stereo = false;
    cfg.left_channel = false;
    cfg.use_adc = false;
  }
  // EchoBase pins/codec callback come from external_speaker.atomic_echo.
  // M5Unified duplicates mono capture into MicLipSync's stereo output buffer.
  M5.Mic.config(cfg);
}
}
