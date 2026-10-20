#!/bin/bash
set -euo pipefail
if [[ -f /etc/default/modulellm-vowel ]]; then
  # Administrator-owned configuration, not an incoming UART payload.
  source /etc/default/modulellm-vowel
fi
UART_DEVICE=${UART_DEVICE:-/dev/ttyS1}
MIC_DEVICE=${MIC_DEVICE:-hw:0,0}
MIC_CHANNEL=${MIC_CHANNEL:-right}
MODEL_FILE=${MODEL_FILE:-/var/lib/modulellm-vowel/model.txt}
here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
# Changing gain/mux/ALC or the selected input invalidates the saved model.
# Do not silently substitute a profile when the mixer cannot be read.
mixer=$(amixer -c 0 contents)
profile=$(printf '%s\n%s\n' "$MIC_DEVICE|S16_LE|16000|stereo|$MIC_CHANNEL|roots-v2" "$mixer" | sha256sum)
profile=${profile%% *}
exec "$here/modulellm-vowel" --capture "$MIC_DEVICE" --channel "$MIC_CHANNEL" \
  --profile "$profile" --model "$MODEL_FILE" --uart "$UART_DEVICE" "$@"
