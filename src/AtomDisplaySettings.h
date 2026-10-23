#pragma once
#include <Preferences.h>
#include <stdint.h>

namespace atomDisplay {
// Screen orientation belongs to the device, independently of microphone models.
inline uint8_t loadRotation() {
  Preferences prefs;
  if (!prefs.begin("atom-display", false)) return 0;
  const uint8_t rotation = prefs.getType("rotation") == PT_U8
      ? prefs.getUChar("rotation", 0) : 0;
  prefs.end();
  return rotation < 4 ? rotation : 0;
}

inline bool saveRotation(uint8_t rotation) {
  if (rotation >= 4) return false;
  Preferences prefs;
  if (!prefs.begin("atom-display", false)) return false;
  const bool saved = prefs.putUChar("rotation", rotation) == sizeof(rotation);
  const bool verified = saved && prefs.getUChar("rotation", 255) == rotation;
  prefs.end();
  return verified;
}
}
