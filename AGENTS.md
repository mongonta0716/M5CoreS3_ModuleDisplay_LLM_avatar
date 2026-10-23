# Remote build and hardware verification

The session resumed on 2026-10-18 to implement ModuleLLM microphone formant
analysis and UART lip sync. Read `README.md`, `modulellm/README.md` and the actual
sources when resuming. The user's goal is natural Avatar lip sync, not
recognition of all Japanese kana. Use a locally configured SSH host to access
ModuleLLM; `modulellm-host` in examples is a placeholder. Inspect installed
audio/UART services before use.

## AtomS3 / AtomS3R frontend

`env:AtomS3` remains the original AtomS3R build for compatibility, and
`env:AtomS3R` is its explicit alias (8MB flash, Octal PSRAM via qio_opi,
128x128 LCD), using boards/m5stack-atoms3r.json based on M5Stack's M5Utility
definition. `env:AtomS3-noPSRAM` targets the original AtomS3 with the standard
m5stack-atoms3 board, qio_qspi SDK, qio flash and no BOARD_HAS_PSRAM define.
USB CDC is explicitly enabled. The board macro selects the startup name and
M5Unified fallback board; do not treat an AtomS3 as an AtomS3R. Both use the
same audio frontend and 128x128 renderer. M5GFX's SpriteBuffer falls back from
PSRAM to internal DMA memory when PSRAM is absent, so no rendering library
change is required. Atom memory logs report free/minimum/largest internal
8-bit heap and total PSRAM at initialization, ready and calibration events.
Its complete build_flags list deliberately overrides the
CoreS3 common ModuleDisplay flag and forces both module flags to zero. The
legacy source filter must exclude atom_s3.cpp to avoid duplicate setup/loop.
Boot probes EchoBase's ES8311 at 0x18 on SDA38/SCL39 before M5.begin. ACK selects
M5Unified external_speaker.atomic_echo; otherwise configure PDM RX on I2S0,
CLK1/DATA2. PDM selection is a fallback, not proof of physical presence.
Keep M5Unified as the sole I2S owner; its mono capture is duplicated by record()
into MicLipSync's stereo output buffer. Stop the startup EchoBase chime before
starting capture. ES8311 defaults to its left slot: set mic.left_channel=true
for EchoBase and speaker.stereo=true to send the mono chime into both slots.
The first version used M5Unified's default right-only I/O; EchoBase ACK and
register writes succeeded, but the user reported no sound or lip sync. The
user confirmed both work after the slot correction. PDM keeps its own input
channel setting. PDM has no startup speaker output.
A single release starts calibration after a 350ms double-click window. A second
press within that window rotates on release, with no Cal before or afterwards.
A 3000ms hold (including a second press) takes priority, resets the selected
profile once and suppresses calibration/rotation on release. CalibrationButton
has native ASan/UBSan tests for click/hold priority, timing boundaries, delayed
polling and rollover. AtomDisplaySettings stores rotation 0..3 as an NVS byte
in namespace atom-display, key rotation. Invalid/missing data defaults to 0.
Restore orientation before the splash. Rotation only writes this key, never
vowel models; resetting a vowel model does not clear screen orientation.
The Atom frontend owns DualDisplayAvatar through unique_ptr and recreates the
renderer on rotation, retaining expression and demo state and preserving the
active calibration. Free the old renderer before allocating new framebuffers;
if registration fails, restore the previous orientation. On 2026-10-23 the
user confirmed on AtomS3+PDMUnit that double clicks rotate without starting
Cal, a nonzero saved orientation returns after USB power cycling for both the
splash and Avatar, and lip sync still works. Serial recorded rotations to
90 and 180 degrees with NVS=OK, with the same 300,632-byte internal free heap
after each. Native gesture tests passed, all three Atom environment names
built, and all four CoreS3 variants built using a temporary config without
the user's common display override. AtomS3R rotation is build-verified only.
Per-input NVS keys atom-echo and
atom-pdm preserve CoreS3's default model key. A reset affects only the selected
input. CalibrationText shares the prompts with CoreS3; Atom renders a dedicated
readable calibration screen rather than the fixed-size two-line face balloon.
Do not flash a connected CoreS3 while verifying this frontend: identify the
connected Atom model first. Build AtomS3 and the four CoreS3 environments after changes
to shared calibration/audio code. AtomS3 and all four CoreS3 combinations
built successfully (the compatibility check used a temporary config without
the user's common display override). Native ASan/UBSan tests passed.
AtomS3R+EchoBase was flashed through normal PlatformIO upload, preserving NVS.
Serial confirms 128x128 LCD, 8MB PSRAM, EchoBase ACK, continuous capture and
about 15.2fps rendering. The user confirmed display, startup audio and lip sync
after the slot correction. The user also confirmed on 2026-10-22 that A's
3-second reset, suppression of calibration on release, short-press calibration,
readable prompts, saving and return to lip sync all work on AtomS3R+EchoBase.
This verifies the operation flow, not a measured vowel-recognition accuracy.
PDMUnit was subsequently verified on original AtomS3 (see below). Preserve
the working firmware and saved calibration; do not reset NVS or publish
personal model values.

On 2026-10-23 the original AtomS3 no-PSRAM environment and both AtomS3R names
built successfully. Before rotation support, AtomS3 static RAM was
69,468 / 327,680 bytes (21.2%); this
excludes dynamic allocations. The renderer adds about 2KiB of 1-bit canvas
and 2KiB of DMA strip on the 128x128 target. The user connected original
AtomS3+PDMUnit, and the no-PSRAM firmware was uploaded normally, retaining NVS.
Boot logs confirm board type 11, 128x128 LCD, PSRAM=0, PDM capture on I2S0
(CLK1/DATA2), nonzero PCM and mouth response, and about 15.2fps / 22.7ms drawing.
At ready: internal 8-bit free=300,768 bytes, minimum=295,640 and largest=270,324.
These are heap measurements at initialization, not a calibration-session peak
measurement. PlatformIO static RAM and runtime heap cover different memory
regions; do not derive runtime free heap by subtracting from the 320KiB display.
The user confirmed display, lip sync, A-started calibration prompts, saving and
return to lip sync on 2026-10-23. This verifies functionality on this combination,
not vowel-recognition accuracy or every AtomS3/EchoBase/PDM combination.
The initial serial monitor saw one ENXIO disconnect. The port reappeared;
a subsequent 20-second read without resetting showed frames 4,700 through
5,954 and continuous rendering, with no further disconnect or panic observed.
The cause of the initial disconnect is unknown; this is not evidence of OOM.
Do not upload to an unidentified connected board or reset its saved model.

## Module selection (2026-10-20)

`USE_MODULELLM` selects the microphone backend and `USE_MODULEDISPLAY` selects
HDMI video/audio. Both default to 0 in `src/AppConfig.h`; firmware features and
physically attached modules are independent. With ModuleDisplay disabled, do not construct or
initialize its SPI/I2C display; use the CoreS3 LCD and M5.Speaker for the chime.
Both speaker modes share I2S1 with the internal microphone: stop/join the mic
before playing and stop the speaker before resuming capture. `Chime.h` selects
the speaker. Print both flag values at startup to detect build-flag overrides.
Build the four environments `m5stack-cores3`, `m5stack-cores3-display`,
`m5stack-cores3-modulellm-vowel`, and `m5stack-cores3-modulellm-display` when
changing shared audio/display integration. The HDMI diagnostics force the display
flag to 1; the old `m5stack-cores3-legacy` remains an independent diagnostic app.

The first standalone CoreS3 investigated on 2026-10-20 was a different unit from
the one used for the successful dual-display microphone tests. Both the normal
app and a minimal M5.Mic.record test received all-zero PCM. Direct I2S reads also
returned zeros; clocks were present but no transitions were observed on GPIO14.
Temporary register/power experiments were removed; retain the normal M5Unified
codec initialization. Preserve NVS.
After uploading a firmware for user observation, keep it running until the user
answers; do not replace it with a diagnostic while a display/audio question is
pending.
The user confirmed the standalone display is normal but the internal chime is
inaudible, unchanged after a complete power cycle. A presses are received.
The amplifier's configuration reads back correctly, and during playback its
SYSST is 0x0311 (clock lock / amplifier switching). PCNT observed BCK/LRCK edges
but no GPIO13 data edges, including a standard one-second M5.Speaker.tone test;
the source PCM has nonzero amplitude and the GPIO13 matrix selects I2S1.
These observations do not establish a hardware defect. A proposed I2S0
comparison failed to compile before any upload and was removed. The user
suspected that unit and replaced it with the previously working CoreS3.

The replacement runs the normal `USE_MODULELLM=0 USE_MODULEDISPLAY=0` app. The
user confirmed sound works but lip sync does not; PCM peak is still zero.
Unlike the first unit, the replacement is stacked with ModuleLLM (user confirmed
after upload); do not assume it is standalone. Capture on I2S0 also gave zeros,
as did starting capture without the startup chime. Both experiments were removed
and the full normal app restored. M5Unified's installed source matches the
official 0.2.26 tag. After powering off and removing ModuleLLM, the user confirmed
both sound and lip sync work; serial capture also shows nonzero PCM. The failed
stacked test used only the CoreS3 USB supply, with no USB power to ModuleLLM,
while `cfg.output_power=false` disables CoreS3 M-Bus power. Thus the replacement
is functional; investigate the attached module's power/shared signals rather
than assuming a microphone defect. With the same normal firmware and ModuleLLM
separately USB powered, the user confirmed sound and lip sync also work.
Thus the unpowered attached ModuleLLM was the reproducible trigger. The exact
electrical mechanism/shared signal is unverified; do not assert an I2C clamp or
specific pin collision. The user then explicitly requested restoring
`cfg.output_power=true`, matching M5Unified's default and the old diagnostic
app. Log the requested and actual M-Bus output state; preserve M5Unified's power
protection and do not force the enable with direct register writes. The user
confirmed that with `USE_MODULELLM=0 USE_MODULEDISPLAY=0` and CoreS3 USB power
alone, ModuleLLM starts and startup/A audio, lip sync and resumption all work.
This replaces the earlier blanket requirement for separate ModuleLLM USB power.
However, serial capture after that confirmation still reports
`setExtPower(true) is canceled` and `M-Bus power: requested=on actual=off`, while
PCM is nonzero. The setting is true, but the output-state reading is not on;
do not claim that the enable latch was verified or infer the exact supply path.
Separate ModuleLLM USB power or removal remains a verified workaround when
M-Bus output is not enabled.
`USE_MODULELLM=0` selects the CoreS3 backend; it does not electrically isolate an
attached ModuleLLM. Do not erase the existing personal calibration.

The latest-official comparison on 2026-10-20 uses M5Unified 0.2.26 (already
installed; master is the same release) and temporarily sets output_power=false.
The user confirmed ModuleLLM stays off, A is audible, and lip sync stays stopped
even after A. Raw serial reads show continuing all-zero PCM; one earlier startup
capture was nonzero, so startup/monitor/power history remains a variable.
The new `enable_latch` log label is intentional: getExtOutput reads BUS_OUT_EN,
not the physical bus voltage. Additional power probes only read registers.
Official CoreS3 issue #72 and M5Unified PRs #352/#381 describe related power-path
protection and switching problems, but do not establish this exact mic failure.
See diagnostics/README.md for sourced comparisons and remaining uncertainties.

## Rendering library integration (2026-10-22)

The four normal environments now fetch M5Unified-Avatar from GitHub, pinned to
`e92d62012a015511567e8ae76f042428e018a6d8`. Do not reintroduce a sibling-directory
symlink as the default dependency. The legacy diagnostic retains its old pin.
`DualDisplayAvatar` only adapts commands, demo/performance logs and DSP labels to
`m5avatar::MultiDisplayAvatar`; vowel poses, balloons, scaling and multiple-screen
rendering live in the library. Its native tests cover pose math and real M5GFX
framebuffers, Japanese text, clip restoration and reserved button rows.
The CoreS3 local-mic/local-display firmware was flashed with output_power=true.
The user confirmed normal Avatar/balloon display, lip sync, A audio and resumption
on 2026-10-22. Earlier output_power=false observations remain historical evidence.
The current platformio.ini also has USE_MODULEDISPLAY=1 in the common build
flags, overriding the header's display default for inheriting environments.
Preserve the user's working setting; remove this common override before using
the documented standalone environment matrix.

## ModuleLLM backend (2026-10-18)

See `modulellm/README.md` for the verified device and build/deployment/rollback.
Both backends are integrated into main. `USE_MODULELLM` in `src/AppConfig.h`
selects the microphone backend at compile time: 0 (default) is CoreS3, 1 is
ModuleLLM over UART. Always use numeric `#if USE_MODULELLM`, not `#ifdef`.
The `m5stack-cores3-modulellm-vowel` environment overrides it with
`-DUSE_MODULELLM=1`; `m5stack-cores3` and its `m5stack-cores3-hello` alias use
the header default. The old app is now `m5stack-cores3-legacy`. No app reads
filesystem assets: all environments disable SPIFFS bundling, avoiding the
Intel-only mkspiffs executable on Apple Silicon during ordinary build/upload.
Both environments must build after backend changes. No hardware was flashed
or remote services changed for this integration.
CoreS3 RX=18 / TX=17 is confirmed by the M5Unified pin table, official example
and actual bidirectional UART. Earlier prose reversing these pins was wrong.
The earlier ModuleLLM verification used a different CoreS3 with its microphone disabled.
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

The current default PlatformIO environment is `m5stack-cores3`, an Avatar
app in `src/hello_world.cpp`. It shows Hello World for 2000 ms, then renders an
animated Avatar using `DualDisplayAvatar`. It also initializes CoreS3's LCD and
touch through `M5.begin()` and uses M5Unified TouchButtons in the bottom 40 px.
After the splash, CoreS3 shows the same Avatar state in a 320x200 viewport above
the buttons (about 267x200 face). With `USE_MODULEDISPLAY=1`, HDMI renders a 240x180 face at logical 320x180,
then ModuleDisplay enlarges it 4x for a 960x720 face on 1280x720@60 output.
`m5stack-cores3` and `m5stack-cores3-hello` set `AVATAR_HDMI_SCALE=4`;
the older `m5stack-cores3-legacy`
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
BtnA and startup play a two-note chime on the selected speaker. With ModuleDisplay
enabled, `Chime` configures a separate Speaker_Class for I2S1, BCK=6, DATA=13,
LRCK=0, 48 kHz stereo; otherwise it uses M5.Speaker and its CoreS3 codec callback.
The CoreS3 internal mic is enabled for FFT lip sync when `USE_MODULELLM=0`.
IMU, RTC and external display autodetection are disabled.
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
excluded from the default build. The original dual-display Avatar diagnostic
build is `m5stack-cores3-legacy`.

This project is built on the development Mac via VS Code Remote SSH. CoreS3 is
connected to another Mac and flashed/monitored through the browser using
`~/work/web_flash_tool`. A missing local USB port does not mean
the user's device is disconnected from their browser Mac.

As of 2026-10-17, the Hello World environment uses M5GFX 0.2.33 and software
I2C (port -1) through `src/CoreS3ModuleDisplay.h`. The pins, SPI configuration,
and video timing match stock M5ModuleDisplay. The user confirmed visible
Hello World output with this combination. `m5stack-cores3-hdmi-hw` runs the stock
hardware-I2C path; `m5stack-cores3-hdmi-diag` adds page-content readback after
failed initialization. Neither is the default. No environment bundles
unused SPIFFS, including the original diagnostic app. See
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
