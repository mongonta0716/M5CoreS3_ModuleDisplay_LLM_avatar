# Remote build and hardware verification

The session resumed on 2026-10-18 to implement ModuleLLM microphone formant
analysis and UART lip sync. Read `README.md`, `modulellm/README.md` and the actual
sources when resuming. The user's goal is natural Avatar lip sync, not
recognition of all Japanese kana. Use a locally configured SSH host to access
ModuleLLM; `modulellm-host` in examples is a placeholder. Inspect installed
audio/UART services before use.

## ModuleLLM backend (2026-10-18)

See `modulellm/README.md` for the verified device and build/deployment/rollback.
Both backends are integrated into main. `USE_MODULELLM` in `src/AppConfig.h`
selects the microphone backend at compile time: 0 (default) is CoreS3, 1 is
ModuleLLM over UART. Always use numeric `#if USE_MODULELLM`, not `#ifdef`.
The `m5stack-cores3-modulellm-vowel` environment overrides it with
`-DUSE_MODULELLM=1`; `m5stack-cores3-hello` uses the header default.
Both environments must build after backend changes. No hardware was flashed
or remote services changed for this integration.
CoreS3 RX=18 / TX=17 is confirmed by the M5Unified pin table, official example
and actual bidirectional UART. Earlier prose reversing these pins was wrong.
CoreS3 has been flashed with the ModuleLLM backend; its microphone is disabled.
AX630C device: Ubuntu 22.04.5 aarch64/glibc 2.35, g++ 11.4.0. No StackFlow audio
or UART services were running. ALSA hw:0,0 requires 16 kHz S16_LE stereo; mono
fails. Use arecord as the capture child, right channel by default, without
changing mixer settings. /dev/ttyS1 is the application UART; leave ttyS0/getty
alone. The service refuses an already-owned UART instead of stopping its owner.
The installed service is `modulellm-vowel.service` (enabled), with current link
`/opt/modulellm-vowel/current` pointing to release `20261018T065303Z-3cac291`.
Model data belongs in `/var/lib/modulellm-vowel/model.txt`, not CoreS3 NVS.
The input/mixer fingerprint must match on startup; B recalibrates the new mic.
Use `bash scripts/deploy_modulellm.sh modulellm-host` to stage/build a new release;
it does not activate it. Do not overwrite an existing remote Git checkout.
Protocol is bounded tab-separated UTF-8 text + CRC32, documented in modulellm
README and shared `src/VowelWire.h`. Result includes Japanese vowel text and
opening/confidence/calibration state. No PCM is sent over UART. Session, control
epoch and sequence reject stale results; timeout 500 ms closes the mouth, and
1 s without accepted results resynchronizes. A/startup mute is local immediately
and also sent remotely; resume follows HDMI DMA completion + 200 ms guard.
Native ASan/UBSan tests (`bash scripts/test_native.sh`) and process integration
tests (`python3 tests/test_modulellm_process.py modulellm/modulellm-vowel`) pass.
The process test also passed on AX630C. Same PCM/164 results: no vowel label
differences or opening differences, maximum confidence delta 0.0001 vs Mac.
This is synthetic parity, not measured real-speaker recognition accuracy.
Actual microphone streaming and physical UART fixed-vowel/pause/resume were
verified. CoreS3 startup, both renderers, valid touch ID, ~9.7 fps while receiving,
and silent waiting after sender stopped are logged in
`diagnostics/2026-10-18-modulellm-core.log` (ignored). Module service metrics are
in its journal. Initial live test: 373 16-ms inputs/6 s, mean 105 us/input,
max 308 us, no audio drop/result replacement. The user confirmed both screens'
fixed-vowel/closed-mouth test and A chime/resumption, then successful real-mic B
calibration/save, vowel shapes/text on both screens and A chime/resumption.
The service logged calibration_save=OK; the model is profile-bound. A controlled
service restart logged model=loaded and a new CoreS3 resume session; the model file
hash was unchanged. Personal model values and hashes are not published.
Do not reset this new personal model to run tests: reset/cancel/idempotence are
tested using temporary model files and a PTY instead. A 216-second live run had
13,501 16-ms inputs, mean DSP 99 us/input (max 1,932 us), 479,892 UART bytes, no
audio drops/queued-result replacement; systemd recorded 2.766 s total CPU time.

The current default PlatformIO environment is `m5stack-cores3-hello`, a dual-display Avatar
app in `src/hello_world.cpp`. It shows Hello World for 2000 ms, then renders an
animated Avatar using `DualDisplayAvatar`. It also initializes CoreS3's LCD and
touch through `M5.begin()` and uses M5Unified TouchButtons in the bottom 40 px.
After the splash, CoreS3 shows the same Avatar state in a 320x200 viewport above
the buttons (about 267x200 face); HDMI renders a 240x180 face at logical 320x180,
then ModuleDisplay enlarges it 4x for a 960x720 face on 1280x720@60 output.
`m5stack-cores3-hello` sets `AVATAR_HDMI_SCALE=4`; the older `m5stack-cores3`
environment remains at 1. `CoreS3ModuleDisplay` reads dimensions and scale from
`AppConfig.h` and explicitly configures the 720p output timing/scaling.
`Avatar perf` logs the completed dual-display FPS and mean draw time every ~5 s.
The scale-2 build was flashed on 2026-10-17; saved startup/performance capture
`diagnostics/2026-10-17-hdmi-scale2.log` (ignored) confirms logical 640x360,
hardware scale 2, a 480x360 HDMI face, both targets ready, and about 5.8 FPS /
172 ms mean draw time for both displays. The user confirmed normal display,
motion, BtnA audio and lip-sync resumption with this build.
The current scale-4 build was subsequently flashed on 2026-10-17; capture
`diagnostics/2026-10-17-hdmi-scale4.log` (ignored) confirms logical 320x180,
hardware scale 4, a 240x180 HDMI face, both targets ready, and about 10.3 FPS /
96.6 ms mean draw time for both displays (about 1.8x the scale-2 FPS).
The user confirmed normal display, motion, BtnA audio and lip-sync resumption
with scale 4 too. Hello World uses fractional text scaling to retain its size.
The renderers share one animation state and run sequentially. Per-display clipping protects the
buttons and restores the previous clip rectangle after each frame.
The user confirmed both screens animate correctly and TouchButton A still plays
the chime. The startup capture is `diagnostics/2026-10-17-dual-avatar.log`
(ignored by Git).
BtnA plays a two-note chime through ModuleDisplay HDMI; startup plays it too.
`HdmiChime` configures a separate Speaker_Class for I2S1, BCK=6, DATA=13, LRCK=0,
48 kHz stereo. The CoreS3 internal mic is enabled for FFT lip sync; internal
speaker, IMU, RTC and external display autodetection are disabled.
`MicLipSync` records 16 kHz stereo, runs avatar-mic's 256-point FFT on released
buffers, and shares the resulting mouth ratio between both render targets.
FFT bins 5..60 cover 312.5..3750 Hz. Source and MIT license are vendored in
`lib/AvatarMicFFT`; native DSP tests are in `tests/test_mic_fft.cpp`.
Vowel lip sync now adds `VowelAnalyzer`: two 256-pair input buffers make a 32 ms
frame, anti-alias FIR decimation to 8 kHz, voicing correlation (80..400 Hz),
pre-emphasis, Hamming window and order-10 autocorrelation LPC. `LpcFormants`
extracts polynomial roots (bounded 80-iteration Durand-Kerner), converts poles
to frequency/bandwidth, and rejects narrow tones / broad slope-fitting poles.
This replaces local envelope maxima, which can merge F1/F2 and select F3.
Initial centers are tunable heuristics; low-confidence or
unvoiced input falls back to FFT amplitude. Two-frame label confirmation and
shared time-smoothed `MouthPose` keep both `VowelMouth` render targets consistent.
Following feedback that A/U/E looked alike, the pose mapping emphasizes a
large tall A, small rounded U, and wide shallow rectangular E. Accepted
confidence >=0.25 maps to 0.85..1 visual strength; weak held labels fade out.
Width/roundness retain vowel identity at modest levels; energy mainly adjusts
height with a smooth closure below 0.06 opening. Quantized 0.75x-canvas tests
check separation at several levels/confidences, including quiet speech.
`VowelMouth` also draws a scaled speech balloon beside the mouth with Japanese
あ/い/う/え/お, or … for silence/uncertainty/demo, using `MouthPose::label`.
The label and shape share the same per-frame state on both displays. Avoid
upstream `Balloon`'s fixed coordinates / M5.Lcd text-state side effects;
draw the balloon in each face canvas so it clears and clips with the face.
RMS below the room-noise gate closes the mouth. Large DSP buffers are members,
not capture-task stack allocations; publish features under a short portMUX,
and only replace the vowel model after stopping/joining the mic task.
BtnB / serial `c` starts calibration: quiet 2 s, then A/I/U/E/O 3 s twice each,
with 2 s breath breaks between every utterance (including vowel changes).
Total time is ~50 s. Combine the two utterances of each vowel; exclude rests
and the first 500 ms of each utterance. Prompts show the vowel and repetition
number, or 息つぎ, plus the remaining time. Native tests check the 3/2/3 timing,
between-vowel rests, exclusion of voiced breath frames and millis rollover.
Both displays show Japanese calibration prompts/countdowns in the Avatar's
upper-right balloon (24 px text, two short lines), then completion/failure/
cancellation for 6 s. Normal
mode shows the inferred vowel beside the mouth. The former LCD-only top banner
has been removed. BtnA cancels and plays PiPo;
BtnC / serial `x` resets calibration. Valid, distinct centers from >=12 voiced
frames/vowel are saved to Preferences namespace `vowel-lipsync`, key `model`,
with magic/version/size/range checks. Version 2 uses LPC roots; version-1 data
is preserved/readable but requires B recalibration for the new extractor.
Quiet samples use a median. Vowel centers are observed pairs nearest the
two-dimensional median, require >=65% inliers, and must classify >=65% of each
vowel's collected frames correctly before saving. Failure preserves the prior
model. Stable synthetic tests include overlapping resonances, low-F2 U, noise
spikes, occasional F3 assignment, and unstable calibration rejection.
Feature extraction runs above a fixed RMS floor of 20, independently of the
previous model's noise gate. Classification still applies that gate, while
calibration uses its newly measured quiet level. This prevents an old high
noise threshold from blocking weak I/U during recalibration. A native
regression checks that a high old gate suppresses labels but retains features.
The first roots calibration attempt logged I below the old noise gate,
explaining why those frames were excluded before this fix.
Model/room noise persist across reboot and chimes; do not reset NVS while
testing unless explicitly using the reset-calibration function.
`tests/test_vowel.cpp` tests synthetic A/I/U/E/O at F0 100/150/220 Hz, rejects
silence/noise/single tones, checks mouth poses and calibration success/failure,
cancellation and millis rollover, with ASan/UBSan. This is not a measured
accuracy result for real Japanese speakers. See README for limitations.
The vowel build's corrected smoothing path was flashed on 2026-10-17; capture
`diagnostics/2026-10-17-vowel-lipsync-fixed.log` (ignored) confirms both targets
render, FFT capture continues, LPC costs about 1..2 ms/frame, and roughly
10.1 FPS / 98 ms mean draw time. The user subsequently confirmed successful
calibration and saving, visible vowel-dependent mouth shapes, BtnA audio and
lip-sync resumption on 2026-10-17. Later startup capture
`diagnostics/2026-10-17-vowel-balloon.log` confirms the saved model loads from
NVS after flashing/reboot, both render targets run, and about 10.1 FPS / 98 ms
mean draw time with the stronger poses and Japanese vowel balloon.
The user confirmed the Japanese labels match the spoken vowels on both
displays and the stronger A/U/E mouth shapes are easier to distinguish.
The user later reported A/I/U mostly show ellipsis; earlier visual confirmation
is not an accuracy validation. `diagnostics/2026-10-17-vowel-diagnosis.log`
shows saved v1 centers differing substantially from later voiced estimates.
The personal calibration values are omitted from this public record. F2/F3 assignment
and unstable calibration are suspected, not proven for every utterance.
The root extractor/robust v2 calibration and calibration balloons are being
tested; actual A/I/U improvement requires user recalibration/observation.
During this iteration the user reported unreadable/missing calibration prompts,
then no touch response. `M5.setTouchButtonHeight(40)` is explicit. Diagnostics
showed FT6336 ID/firmware/vendor all zero before HDMI initialization, via
hardware I2C, Wire1 and software I2C alike; the same software probe read AW9523
ID 0x23. Rebinding the HW pins and reinitializing the touch driver did not fix
it. `recoverTouch()` now pulses TOUCH_RST (AW9523 P0_0) only when the startup
vendor read is invalid, preserving every other output, then initializes touch.
After flashing this version, the next captured boot reads 64/01/01 before and
after HDMI, and logs a B touch at x=153/y=239 and calibration start. The first
post-flash recovery itself was not captured, so do not claim its return values
were logged. `diagnostics/2026-10-17-vowel-touch-routing.log` contains several
builds; use the Build marker to distinguish them. The user confirmed A/B
recovered and the prompts are readable on 2026-10-17. Real vowel accuracy is
still unresolved; subsequent A stages rejected variable F1 estimates and kept
the old model. Both displays remain ~9.7..10.1 FPS. The user could not sustain
6 s, so the current calibration uses 3 s twice with breath breaks instead.
The 3-second version (Build Oct 17 2026 17:07:26) was flashed successfully;
`diagnostics/2026-10-17-vowel-three-second-rounds.log` confirms valid touch IDs,
both Avatar targets, active microphone capture and ~10.0..10.1 FPS at startup.
Native vowel/calibration tests pass with ASan/UBSan for the two-utterance flow.
In that capture, a subsequent B run completed all ten utterances with breath
breaks and saved the v2 model to NVS, replacing the earlier v1 centers.
Personal calibration values remain on the device and in ignored local logs.
The run validates the calibration flow and save, not general speech accuracy.
The user confirmed the 3 s x 2 flow and save, initially reported many ellipses,
then subsequently confirmed A/I/U/E/O can be distinguished. This is a user
observation, not a measured accuracy rate. Voiced rendering was ~9.7 FPS and
quiet rendering ~10 FPS.
The first attempt (`...-vowel-lipsync.log`)
stalled on unsigned negation in the pose smoother; the fix casts elapsed time
to float before negation. Native smoothing tests cover 0, typical, long and
rollover-sized unsigned durations. Do not mistake the first log for this build.
CoreS3 mic uses I2S1 (MCLK=0, BCK=34, WS=33, DATA=14), sharing both I2S1 and
GPIO0 with HDMI audio. Stop/join the mic before starting the HDMI speaker;
stop the speaker after the chime and DMA tail before restarting the mic.
Never run both audio drivers concurrently or move just one to another I2S port:
GPIO0 still conflicts. Capture callbacks only analyze/requeue at the same rate;
audio lifecycle and rendering stay on the main task. Serial `t` overrides mic
lip sync with the existing sine-wave mouth demo until toggled off.
The FFT build was compiled and flashed to the locally attached CoreS3 on
2026-10-17. `diagnostics/2026-10-17-mic-fft.log` (ignored) confirms startup
chime queuing, mic startup afterwards, both Avatar targets ready, and continuing
FFT frames with nonzero PCM/FFT levels. Native DSP tests pass with ASan/UBSan.
The user subsequently confirmed that lip sync on both displays and BtnA's
chime/resumption work normally on 2026-10-17.
ModuleDisplay must be externally powered. Custom I2C probes are
excluded from the default build. The original dual-display Avatar build remains
`m5stack-cores3`.

This project is built on the development Mac via VS Code Remote SSH. CoreS3 is
connected to another Mac and flashed/monitored through the browser using
`~/work/web_flash_tool`. A missing local USB port does not mean
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
MacBook. Enumerate ports before using one; `/dev/cu.usbmodemXXXX` is a
placeholder and the actual name can change.
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
Read `~/work/web_flash_tool/AGENTS.md` for the log format and
workflow. The most recently updated saved session can be read with:

```sh
node ~/work/web_flash_tool/scripts/read_serial_log.js --latest
```

Logs are in `~/work/web_flash_tool/logs/<session-UUID>.jsonl`.
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
- The initial diagnostic session reports M-Bus output enabled,
  HDMI chip ID `17 02 e2`, FPGA ID `48440004`, and failure selecting bank `81`.
  Banks `80`, `82`, and `84` succeed with HW I2C at 100 kHz / 25 kHz and SW I2C
  at requested 25 kHz. All three methods still read the correct chip ID.
- `error=2` is M5GFX's generic `connection_lost`, not a precise NACK diagnosis.
  Do not infer a defective chip, insufficient power, or a hardware-I2C-only bug
  from this evidence alone. Earlier M-Bus-disabled warnings are not necessary
  for the failure to occur. Adapter voltage remains unconfirmed.
- The subsequent diagnostic session ran the first Hello World test
  using M5Unified's standard display initialization. It reproduced the bank-81
  failure with no Avatar linked, before any text could be drawn. The next test
  removes M5Unified too (startup marker includes `M5GFX only`).
