"""Capture CoreS3 verification logs; use --stop-file to end without a reset."""
import argparse
import pathlib
import time
import serial

parser = argparse.ArgumentParser()
parser.add_argument("--port", required=True)
parser.add_argument("--log", required=True)
parser.add_argument("--duration", type=float, default=240)
parser.add_argument("--stop-file", default="/tmp/modulellm-core-monitor.stop")
args = parser.parse_args()
stop = pathlib.Path(args.stop_file)
stop.unlink(missing_ok=True)
port = serial.Serial(port=None, baudrate=115200, timeout=.25)
port.dtr = False
port.rts = False
port.port = args.port
port.open()
try:
    until = time.monotonic()+args.duration
    with open(args.log, "ab", buffering=0) as log:
        while time.monotonic() < until and not stop.exists():
            data = port.read(8192)
            if data:
                log.write(data)
                print(data.decode(errors="replace"), end="", flush=True)
finally:
    port.close()
