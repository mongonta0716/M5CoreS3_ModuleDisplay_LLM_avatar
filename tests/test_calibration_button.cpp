#include "CalibrationButton.h"
#include "AtomAudioProfile.h"
#include <cassert>
#include <cstring>
#include <cstdio>
using Action = CalibrationButton::Action;

int main() {
  {
    CalibrationButton b;
    assert(b.update(false, 0) == Action::None);
    assert(b.update(true, 100) == Action::None);
    assert(b.update(false, 180) == Action::None);
    assert(b.update(false, 529) == Action::None);
    assert(b.update(false, 530) == Action::Calibrate);
    assert(b.update(false, 1000) == Action::None);
  }
  {
    CalibrationButton b;
    assert(b.update(true, 100) == Action::None);
    assert(b.update(false, 180) == Action::None);
    assert(b.update(true, 400) == Action::None);
    assert(b.update(true, 600) == Action::None); // no Cal during second press
    assert(b.update(false, 700) == Action::Rotate);
    assert(b.update(false, 1500) == Action::None); // no delayed Cal after double
    assert(b.update(true, 1600) == Action::None);
    assert(b.update(false, 1650) == Action::None);
    assert(b.update(true, 1700) == Action::None);
    assert(b.update(false, 1750) == Action::Rotate); // another double
  }
  {
    CalibrationButton b;
    assert(b.update(true, 100) == Action::None);
    assert(b.update(false, 180) == Action::None);
    assert(b.update(true, 530) == Action::Calibrate); // exact timeout: separate click
    assert(b.update(false, 600) == Action::None);
    assert(b.update(false, 950) == Action::Calibrate);
  }
  {
    CalibrationButton b;
    assert(b.update(true, 1000) == Action::None);
    assert(b.update(true, 3999) == Action::None);
    assert(b.update(true, 4000) == Action::Reset);
    assert(b.update(true, 9000) == Action::None);
    assert(b.update(false, 9010) == Action::None);
    assert(b.update(false, 9500) == Action::None);
    assert(b.update(true, 10000) == Action::None);
    assert(b.update(false, 13000) == Action::Reset); // delayed polling on release
    assert(b.update(false, 14000) == Action::None);
  }
  {
    CalibrationButton b;
    assert(b.update(true, 100) == Action::None);
    assert(b.update(false, 180) == Action::None);
    assert(b.update(true, 529) == Action::None); // last instant within double gap
    assert(b.update(true, 3528) == Action::None);
    assert(b.update(true, 3529) == Action::Reset); // second hold takes priority
    assert(b.update(false, 3530) == Action::None);
    assert(b.update(false, 4000) == Action::None); // neither Rotate nor Cal
  }
  {
    CalibrationButton b;
    assert(b.update(true, UINT32_MAX - 150) == Action::None);
    assert(b.update(false, UINT32_MAX - 100) == Action::None);
    assert(b.update(false, 248) == Action::None);
    assert(b.update(false, 249) == Action::Calibrate);
    assert(b.update(true, UINT32_MAX - 300) == Action::None);
    assert(b.update(false, UINT32_MAX - 250) == Action::None);
    assert(b.update(true, 50) == Action::None);
    assert(b.update(false, 120) == Action::Rotate);
    assert(b.update(true, UINT32_MAX - 1000) == Action::None);
    assert(b.update(true, 1998) == Action::None);
    assert(b.update(true, 1999) == Action::Reset);
    assert(b.update(false, 2100) == Action::None);
  }
  using namespace atomAudio;
  assert(select(true) == Input::EchoBase && select(false) == Input::PdmUnit);
  assert(std::strcmp(profileKey(select(true)), profileKey(select(false))) != 0);
  assert(std::strcmp(profileKey(select(true)), "model") != 0);
  assert(std::strcmp(profileKey(select(false)), "model") != 0);
  std::puts("Atom gestures: single/double clicks, timeout boundaries, long-press priority, rollover and profiles passed");
}
