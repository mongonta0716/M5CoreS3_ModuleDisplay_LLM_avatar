"""Integration tests for the actual executable, using a PTY and synthetic PCM.

Runs on macOS and ModuleLLM Linux. It never opens a physical UART/microphone.
"""
import argparse
import collections
import math
import os
import pathlib
import pty
import select
import struct
import subprocess
import tempfile
import time
import zlib


def packet(body):
    data = body.encode()
    return data + b"\t%08x\n" % zlib.crc32(data)


def parse(line):
    body, crc = line.rsplit(b"\t", 1)
    assert zlib.crc32(body) == int(crc, 16)
    return body.decode().split("\t")


def fixture(path):
    with open(path, "wb") as out:
        for f1, f2 in [(800, 1200), (300, 2300), (350, 1300), (500, 1900), (500, 850)]:
            poles = []
            for freq, bw in [(f1, 90), (f2, 90), (2900, 140)]:
                r = math.exp(-math.pi * bw / 16000)
                poles.append([2*r*math.cos(2*math.pi*freq/16000), -r*r, 0., 0.])
            raw = []
            phase = 0.
            for _ in range(16000):
                phase += 150/16000
                x = 0.
                if phase >= 1:
                    phase -= 1
                    x = 1.
                for p in poles:
                    y = x + p[0]*p[2] + p[1]*p[3]
                    p[3], p[2], x = p[2], y, y
                raw.append(x)
            peak = max(map(abs, raw))
            for x in raw:
                value = int(x / peak * 12000)
                out.write(struct.pack("<hh", value, value))
        out.write(b"\0" * 16000)  # 250ms true silence, including full closure.


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("binary")
    parser.add_argument("--dump-results")
    args = parser.parse_args()
    binary = str(pathlib.Path(args.binary).resolve())
    with tempfile.TemporaryDirectory(prefix="vowel-process-") as folder:
        pcm = pathlib.Path(folder) / "fixture.pcm"
        model = str(pathlib.Path(folder) / "model.txt")
        fixture(pcm)
        result = subprocess.run([binary, "--input", str(pcm), "--uart", "-", "--profile", "test",
                                 "--channel", "mix", "--model", model], capture_output=True, check=True)
        frames = [parse(line) for line in result.stdout.splitlines()]
        expected = list("あいうえお")
        for vowel, start in zip(expected, [0, 1000, 2000, 3000, 4000]):
            section = [f for f in frames if start+200 <= int(f[4])*32 < start+1000]
            correct = sum(f[5] == vowel for f in section)
            assert section and correct >= len(section)*.7, (vowel, correct, len(section))
        assert frames[-1][5] == "無音" and float(frames[-1][6]) == 0
        if args.dump_results:
            pathlib.Path(args.dump_results).write_text("\n".join("\t".join(f[4:]) for f in frames)+"\n")
        print("PCM fixture:", len(frames), "frames; all five synthetic vowels and silence passed")

        master, slave = pty.openpty()
        device = os.ttyname(slave)
        os.close(slave)
        process = subprocess.Popen([binary, "--uart", device, "--test-pattern", "--profile", "test",
                                    "--model", model, "--seconds", "10"], stderr=subprocess.PIPE)
        buffer = b""

        def command(client, epoch, name):
            os.write(master, packet(f"C1\t{client}\t{epoch}\t{name}"))

        def receive(client, epoch, duration):
            nonlocal buffer
            found = []
            deadline = time.monotonic()+duration
            while time.monotonic() < deadline:
                if not select.select([master], [], [], min(.05, max(0, deadline-time.monotonic())))[0]:
                    continue
                try:
                    data = os.read(master, 4096)
                except OSError:
                    time.sleep(.01)
                    continue
                buffer += data
                while b"\n" in buffer:
                    line, buffer = buffer.split(b"\n", 1)
                    if not line.startswith(b"V1\t"):
                        continue
                    f = parse(line)
                    if int(f[2]) == client and int(f[3]) == epoch:
                        found.append(f)
            return found

        try:
            time.sleep(.15)
            command(123, 1, "resume")
            frames = receive(123, 1, 5.1)
            assert set(expected+["無音", "不明"]) <= {f[5] for f in frames}
            command(123, 2, "pause")
            frames = receive(123, 2, .3)
            assert frames and all(f[5] == "無音" and float(f[6]) == 0 for f in frames)
            command(123, 1, "resume")  # Replay must not cancel the pause.
            frames = receive(123, 2, .2)
            assert frames and all(f[5] == "無音" for f in frames)
            os.write(master, b"bad\n"+b"x"*500+b"\n")
            command(123, 3, "resume")
            assert receive(123, 3, .2)
            command(456, 1, "resume")  # New CoreS3 boot; prior result epoch must not survive.
            assert receive(456, 1, .2)
            command(456, 2, "calibrate")
            frames = receive(456, 2, .2)
            assert frames and all(f[8] == "1" for f in frames)
            command(456, 3, "cancel")
            frames = receive(456, 3, .2)
            assert frames and all(f[8] == "5" for f in frames)
            pathlib.Path(model).write_text("temporary test model")
            command(456, 4, "reset")
            frames = receive(456, 4, .2)
            assert frames and all(f[8] == "6" for f in frames) and not pathlib.Path(model).exists()
            pathlib.Path(model).write_text("must survive duplicate reset")
            command(456, 4, "reset")
            assert receive(456, 4, .2) and pathlib.Path(model).exists()
        finally:
            process.terminate()
            _, errors = process.communicate(timeout=3)
            os.close(master)
        assert process.returncode == 0, errors.decode()
        print("PTY UART: UTF-8 vowels, silence/unknown, pause/resume, replay rejection, oversized input, session restart, calibration/cancel and idempotent reset passed")


if __name__ == "__main__":
    main()
