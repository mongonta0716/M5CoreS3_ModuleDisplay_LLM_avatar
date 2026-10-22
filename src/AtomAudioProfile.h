#pragma once

namespace atomAudio {
enum class Input { EchoBase, PdmUnit };
inline Input select(bool echoResponded) {
  return echoResponded ? Input::EchoBase : Input::PdmUnit;
}
inline const char* profileKey(Input input) {
  // Separate from CoreS3's default "model", and from each other.
  return input == Input::EchoBase ? "atom-echo" : "atom-pdm";
}
inline const char* name(Input input) {
  return input == Input::EchoBase ? "EchoBase" : "PDMUnit";
}
}
