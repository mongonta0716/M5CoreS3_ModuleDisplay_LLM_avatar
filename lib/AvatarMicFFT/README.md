# AvatarMicFFT

`src/fft.hpp` is vendored from
[m5stack-avatar-mic](https://github.com/mongonta0716/m5stack-avatar-mic), commit
`3a486b3fdb4772b59135a42d5446af16dbbaafd9`, under its MIT license (see `LICENSE`).
The upstream file credits M5Unified's Bluetooth_with_ESP32A2DP example.
Local changes add standalone C++ includes, an include guard, and explicit zero
initialization so instances are safe outside static storage. The FFT algorithm
is unchanged: 256 points, Hann window, stereo-to-mono input.
