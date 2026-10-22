#!/bin/bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
host=${1:?Usage: bash scripts/deploy_modulellm.sh SSH_HOST}
release="$(date -u +%Y%m%dT%H%M%SZ)-$(git rev-parse --short HEAD)"
remote_release="/opt/modulellm-vowel/releases/$release"
archive=$(mktemp "${TMPDIR:-/tmp}/modulellm-vowel.XXXXXX")
trap 'rm -f -- "$archive"' EXIT
COPYFILE_DISABLE=1 tar --no-xattrs --exclude=modulellm/modulellm-vowel -czf "$archive" \
  modulellm src/VowelWire.h src/VowelModel.h src/VowelAnalyzer.h src/LpcFormants.h \
  src/VowelCalibration.h src/MicFftAnalyzer.h src/CalibrationButton.h src/AtomAudioProfile.h \
  lib/AvatarMicFFT tests scripts/test_native.sh
ssh "$host" "set -eu
mkdir -p '$remote_release'
tar -xzf - -C '$remote_release'" < "$archive"
ssh "$host" "set -eu
make -C '$remote_release/modulellm'
printf '%s\n' '$remote_release'"
printf 'Candidate release: %s\n' "$remote_release"
printf 'Not activated. Test this release, then follow modulellm/README.md to switch current.\n'
