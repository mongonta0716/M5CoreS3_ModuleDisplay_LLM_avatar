#pragma once

#include <M5GFX.h>
#include <lgfx/v1/panel/Panel_M5HDMI.hpp>

// M5ModuleDisplay's CoreS3 pin/timing configuration, with an explicit I2C
// port so software I2C (-1) can be compared without patching the library.
class CoreS3ModuleDisplay : public lgfx::LGFX_Device {
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
    resolution.logical_width = 1280;
    resolution.logical_height = 720;
    panel_.config_resolution(resolution);
    setPanel(&panel_);
  }

 private:
  lgfx::Bus_SPI bus_;
  lgfx::Panel_M5HDMI panel_;
};
