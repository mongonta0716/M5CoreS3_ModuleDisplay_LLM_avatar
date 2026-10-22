#include "CalibrationButton.h"
#include "AtomAudioProfile.h"
#include <cassert>
#include <cstring>
#include <cstdio>
using Action = CalibrationButton::Action;
int main() {
  CalibrationButton button;
  assert(button.update(false, 0) == Action::None);
  assert(button.update(true, 100) == Action::None);
  assert(button.update(false, 180) == Action::Calibrate);
  assert(button.update(false, 200) == Action::None);
  assert(button.update(true, 1000) == Action::None);
  assert(button.update(true, 3999) == Action::None);
  assert(button.update(true, 4000) == Action::Reset);
  assert(button.update(true, 9000) == Action::None);
  assert(button.update(false, 9010) == Action::None); // release after reset is not Cal
  assert(button.update(true, 10000) == Action::None);
  assert(button.update(false, 13000) == Action::Reset); // delayed polling on release
  assert(button.update(true, UINT32_MAX - 1000) == Action::None);
  assert(button.update(true, 1998) == Action::None);
  assert(button.update(true, 1999) == Action::Reset);
  assert(button.update(false, 2100) == Action::None);
  assert(button.update(true, 2200) == Action::None);
  assert(button.update(false, 2300) == Action::Calibrate);
  using namespace atomAudio;
  assert(select(true) == Input::EchoBase && select(false) == Input::PdmUnit);
  assert(std::strcmp(profileKey(select(true)), profileKey(select(false))) != 0);
  assert(std::strcmp(profileKey(select(true)), "model") != 0);
  assert(std::strcmp(profileKey(select(false)), "model") != 0);
  std::puts("Atom gestures: click, 3s hold, single reset, release, rollover and input profiles passed");
}
