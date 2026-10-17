# Remote build and hardware verification

The current default PlatformIO environment is `m5stack-cores3-hello`, an HDMI Avatar
app in `src/hello_world.cpp`. It shows Hello World for 2000 ms, then renders an
animated Avatar using `DualDisplayAvatar`. It also initializes CoreS3's LCD and
touch through `M5.begin()` and uses M5Unified TouchButtons in the bottom 40 px.
BtnA plays a two-note chime through ModuleDisplay HDMI; startup plays it too.
`HdmiChime` configures a separate Speaker_Class for I2S1, BCK=6, DATA=13, LRCK=0,
48 kHz stereo. Internal speaker/mic, IMU, RTC and external display autodetection
are disabled. ModuleDisplay must be externally powered. Custom I2C probes are
excluded from the default build. The original dual-display Avatar build remains
`m5stack-cores3`.

This project is built on the development Mac via VS Code Remote SSH. CoreS3 is
connected to another Mac and flashed/monitored through the browser using
`/Users/mongonta555/work/web_flash_tool`. A missing local USB port does not mean
the user's device is disconnected from their browser Mac.

As of 2026-10-17, the Hello World environment uses M5GFX 0.2.33 and software
I2C (port -1) through `src/CoreS3ModuleDisplay.h`. The pins, SPI configuration,
and video timing match stock M5ModuleDisplay. The user confirmed visible
Hello World output with this combination. `m5stack-cores3-hdmi-hw` runs the stock
hardware-I2C path; `m5stack-cores3-hdmi-diag` adds page-content readback after
failed initialization. Neither is the default. Hello World does not bundle
unused SPIFFS; the original dual-display Avatar build still does. See
`diagnostics/README.md` for evidence.
The current HDMI Avatar uses the same software-I2C configuration and no
filesystem assets. The user subsequently connected CoreS3 directly to this
MacBook; its observed port was `/dev/cu.usbmodem114301`. Enumerate ports before
using it, since the name can change.
The user also confirmed the Hello World to Avatar transition and visible
animation on HDMI. The serial capture is
`diagnostics/2026-10-17-hello-then-avatar.log` (ignored by Git).
Touch shares SDA=12/SCL=11 with HDMI control. `CoreS3ModuleDisplay::init_impl`
explicitly initializes software I2C and restores the pin routing afterwards so
CoreS3's hardware-I2C touch polling remains usable. Keep HDMI out of automatic
M5Unified display registration so the CoreS3 display remains the touch source.
The user confirmed audible HDMI chimes both at startup and on TouchButton A
on 2026-10-17. Startup/touch/audio initialization logs are saved in
`diagnostics/2026-10-17-hdmi-chime-touch.log` (ignored by Git).
A later silent-audio report was resolved by correcting the macOS/QuickTime
audio output destination. During investigation, BtnA requests and PCM consumption
worked; PCNT measured LRCK about 48 kHz and BCK about 1.536 MHz at the CoreS3
pads. The user confirmed sound after correcting the output destination.
Temporary test-tone, PCNT and EDID diagnostics were removed from the app.

The Web Flash Tool automatically saves logs when the browser's Monitor runs.
Read `/Users/mongonta555/work/web_flash_tool/AGENTS.md` for the log format and
workflow. The most recently updated saved session can be read with:

```sh
node /Users/mongonta555/work/web_flash_tool/scripts/read_serial_log.js --latest
```

Logs are in `/Users/mongonta555/work/web_flash_tool/logs/<session-UUID>.jsonl`.
Check metadata and startup output to associate a session with the correct
device and firmware. If no logs exist yet, the user must reload the updated
Web Flash Tool and start Monitor. Saved logs can verify HDMI initialization,
display registration, and PSRAM allocation; visible HDMI output needs user
observation or camera evidence. Do not flash/reset hardware or restart the
server solely to inspect saved logs.

## HDMI investigation evidence (2026-10-16)

- Quad PSRAM configuration (`qio_qspi`) corrected the original PSRAM failure;
  subsequent logs report about 8 MiB free PSRAM.
- The user reproduced HDMI initialization failure with ModuleLLM removed and
  confirmed that both CoreS3 and ModuleDisplay have power connected.
- Session `dccbb5a1-d1e1-40d7-b4d2-3ee952121302` reports M-Bus output enabled,
  HDMI chip ID `17 02 e2`, FPGA ID `48440004`, and failure selecting bank `81`.
  Banks `80`, `82`, and `84` succeed with HW I2C at 100 kHz / 25 kHz and SW I2C
  at requested 25 kHz. All three methods still read the correct chip ID.
- `error=2` is M5GFX's generic `connection_lost`, not a precise NACK diagnosis.
  Do not infer a defective chip, insufficient power, or a hardware-I2C-only bug
  from this evidence alone. Earlier M-Bus-disabled warnings are not necessary
  for the failure to occur. Adapter voltage remains unconfirmed.
- Session `aaf86d20-4989-4110-a36a-86acd4565409` ran the first Hello World test
  using M5Unified's standard display initialization. It reproduced the bank-81
  failure with no Avatar linked, before any text could be drawn. The next test
  removes M5Unified too (startup marker includes `M5GFX only`).
