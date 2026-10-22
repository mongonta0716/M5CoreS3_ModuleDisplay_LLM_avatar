#pragma once

#include <Preferences.h>
#include "VowelModel.h"

namespace vowelStorage {
struct SavedModel {
  uint32_t magic = 0x564F574C;  // VOWL
  uint32_t version = 2;  // LPC-root calibration; version 1 used envelope peaks.
  VowelModel model;
};
inline VowelModel load(const char* key = "model") {
  Preferences prefs;
  SavedModel saved;
  bool ok = false;
  if (prefs.begin("vowel-lipsync", false)) {
    ok = prefs.isKey(key) && prefs.getBytesLength(key) == sizeof(saved) &&
         prefs.getBytes(key, &saved, sizeof(saved)) == sizeof(saved) &&
         saved.magic == 0x564F574C && (saved.version == 1 || saved.version == 2) && saved.model.valid();
    prefs.end();
  }
  Serial.printf("Vowel model: %s\n", ok ? "loaded from NVS" : "initial centers");
  if (ok && saved.version == 1)
    Serial.println("Vowel model: legacy peak-based centers preserved; recalibrate with B for LPC roots");
  const auto model = ok ? saved.model : VowelModel{};
  Serial.printf("Vowel model: noise RMS=%.1f\n", model.noiseRms);
  for (unsigned i = 0; i < 5; ++i)
    Serial.printf("Vowel model: %s F1=%.0f F2=%.0f\n", vowelName(Vowel(i + 1)),
                  model.f1[i], model.f2[i]);
  return model;
}
inline bool save(const VowelModel& model, const char* key = "model") {
  if (!model.valid()) return false;
  Preferences prefs;
  if (!prefs.begin("vowel-lipsync", false)) return false;
  SavedModel saved;
  saved.model = model;
  const bool ok = prefs.putBytes(key, &saved, sizeof(saved)) == sizeof(saved);
  prefs.end();
  return ok;
}
inline bool reset(const char* key = "model") {
  Preferences prefs;
  if (!prefs.begin("vowel-lipsync", false)) return false;
  const bool ok = !prefs.isKey(key) || prefs.remove(key);
  prefs.end();
  return ok;
}
}  // namespace vowelStorage
