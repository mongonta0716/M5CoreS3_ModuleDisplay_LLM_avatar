#include "HdmiDiagnostics.h"

#include <M5GFX.h>
#include <Arduino.h>

namespace {
constexpr int kSda = 12;
constexpr int kScl = 11;
constexpr int kAddress = 0x39;

void probe(int port, uint32_t frequency, const char* name) {
  Serial.printf("HDMI diag: %s port=%d requested=%u Hz\n", name, port,
                static_cast<unsigned>(frequency));
  // Only change bank selection and enable ID reads, as the stock driver does.
  // No power, PLL, reset, or display-timing registers are changed here.
  for (uint8_t bank : {0x80, 0x81, 0x82, 0x84, 0x80}) {
    const uint8_t data[] = {0xff, bank};
    const auto result = lgfx::i2c::transactionWrite(
        port, kAddress, data, sizeof(data), frequency);
    Serial.printf("HDMI diag: %s bank=%02x write=%s error=%d\n", name, bank,
                  result.has_value() ? "OK" : "FAIL",
                  result.has_error() ? static_cast<int>(result.error()) : 0);
    delay(2);
  }

  const uint8_t enableId[] = {0xee, 0x01};
  auto enabled = lgfx::i2c::transactionWrite(
      port, kAddress, enableId, sizeof(enableId), frequency);
  uint8_t id[3] = {};
  bool valid = enabled.has_value();
  for (uint8_t reg = 0; reg < 3; ++reg) {
    auto result = lgfx::i2c::transactionWriteRead(
        port, kAddress, &reg, 1, &id[reg], 1, frequency);
    valid = valid && result.has_value();
  }
  Serial.printf("HDMI diag: %s ID=%02x %02x %02x reads=%s\n", name,
                id[0], id[1], id[2], valid ? "OK" : "FAIL");
}
}  // namespace

void diagnoseHdmiI2c() {
  Serial.println("HDMI diag: begin (bank selection only; not an HDMI output test)");
  {
    lgfx::i2c::i2c_temporary_switcher_t bus(1, kSda, kScl);
    probe(1, 100000, "HW100k");
    probe(1, 25000, "HW25k");
    bus.restore();
  }
  // No M5.update(), audio tasks or display operations run during this setup
  // probe. Restore the GPIO routing before returning to the shared HW bus.
  lgfx::gpio::pin_backup_t pins[] = {kSda, kScl};
  auto initialized = lgfx::i2c::init(-1, kSda, kScl);
  if (initialized.has_value()) {
    probe(-1, 25000, "SW25k");
    lgfx::i2c::release(-1);
  } else {
    Serial.printf("HDMI diag: software I2C init failed error=%d\n",
                  static_cast<int>(initialized.error()));
  }
  for (auto& pin : pins) pin.restore();
  Serial.println("HDMI diag: end; shared I2C GPIO routing restored");
}
