#!/bin/bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
out=$(mktemp -d "${TMPDIR:-/tmp}/vowel-tests.XXXXXX")
trap 'rm -rf -- "$out"' EXIT
compiler=${CXX:-clang++}
flags=(-std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined -Isrc -Imodulellm -Ilib/AvatarMicFFT/src)
for test in test_mic_fft test_vowel test_vowel_wire test_calibration_button; do
  "$compiler" "${flags[@]}" "tests/$test.cpp" -o "$out/$test"
  "$out/$test"
done
