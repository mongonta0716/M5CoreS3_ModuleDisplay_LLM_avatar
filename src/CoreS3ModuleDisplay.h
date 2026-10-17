#pragma once

#include <M5GFX.h>
#include <lgfx/v1/panel/Panel_M5HDMI.hpp>
#include "AppConfig.h"

// M5ModuleDisplay's CoreS3 pin/timing configuration, with an explicit I2C
// port so software I2C (-1) can be compared without patching the library.
class CoreS3ModuleDisplay : public M5GFX {
 public:
  explicit CoreS3ModuleDisplay(int i2cPort) {
    auto spi = bus_.config();
    spi.freq_write = 80000000;
    spi.freq_read = 16000000;
    spi.spi_mode = 3;
    spi.spi_host = SPI2_HOST;
    spi.dma_channel = SPI_DMA_CH_AUTO;
    spi.use_lock = true;
    spi.pin_mosi = 37;
    spi.pin_miso = 35;
    spi.pin_sclk = 36;
    spi.spi_3wire = false;
    bus_.config(spi);
    panel_.setBus(&bus_);

    auto transmitter = panel_.config_transmitter();
    transmitter.freq_read = 100000;
    transmitter.freq_write = 100000;
    transmitter.pin_sda = 12;
    transmitter.pin_scl = 11;
    transmitter.i2c_port = i2cPort;
    transmitter.i2c_addr = 0x39;
    transmitter.prefix_cmd = 0;
    transmitter.prefix_data = 0;
    transmitter.prefix_len = 0;
    panel_.config_transmitter(transmitter);

    auto panel = panel_.config();
    panel.offset_rotation = 3;
    panel.pin_cs = 7;
    panel.readable = false;
    panel.bus_shared = true;
    panel_.config(panel);
    panel_.setRotation(1);

    lgfx::Panel_M5HDMI::config_resolution_t resolution;
    resolution.logical_width = app::kLogicalWidth;
    resolution.logical_height = app::kLogicalHeight;
    resolution.output_width = 1280;
    resolution.output_height = 720;
    resolution.refresh_rate = 60;
    resolution.scale_w = app::kDisplayScale;
    resolution.scale_h = app::kDisplayScale;
    resolution.pixel_clock = 74250000;
    panel_.config_resolution(resolution);
    setPanel(&panel_);
    _board = lgfx::board_t::board_M5ModuleDisplay;
  }

 protected:
  bool init_impl(bool use_reset, bool use_clear) override {
    // The panel is configured explicitly; skip CoreS3 autodetection and its
    // built-in LCD / power setup, while exposing M5GFX to the Avatar library.
    const int port = panel_.config_transmitter().i2c_port;
    if (port >= 0) {
      return lgfx::LGFX_Device::init_impl(use_reset, use_clear);
    }
    // CoreS3 touch shares SDA/SCL. Explicitly initialize the software port
    // even when the HW port already uses these pins, then restore its routing.
    lgfx::gpio::pin_backup_t pins[] = {12, 11};
    const bool busReady = lgfx::i2c::init(port, 12, 11).has_value();
    const bool ready = busReady && lgfx::LGFX_Device::init_impl(use_reset, use_clear);
    lgfx::i2c::release(port);
    for (auto& pin : pins) pin.restore();
    return ready;
  }

 private:
  lgfx::Bus_SPI bus_;
  lgfx::Panel_M5HDMI panel_;
};
