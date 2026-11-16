#!/usr/bin/env python3
"""Saves the README screenshots from a CoreS3 running the firmware in this folder.

    pio run -d tools/screenshots -t upload
    python3 tools/screenshots/capture.py [--port PORT] [--out DIR] [screen ...]

Needs pyserial (PlatformIO's Python has it). Without screen names, saves every screen the firmware offers.
"""

import argparse
import pathlib
import sys
import time

import serial
from serial.tools import list_ports

ESPRESSIF_VID = 0x303A
ROOT = pathlib.Path(__file__).resolve().parents[2]


def find_port():
    for p in list_ports.comports():
        if p.vid == ESPRESSIF_VID:
            return p.device
    return None


def open_port(port, timeout):
    """Opens the port without touching DTR / RTS (they reset the chip), waiting for it to appear."""
    until = time.time() + timeout
    while time.time() < until:
        device = port or find_port()
        if device:
            s = serial.Serial()
            s.port, s.baudrate, s.timeout = device, 115200, 1
            s.dtr = s.rts = False
            try:
                s.open()
                return s
            except serial.SerialException:
                pass
        time.sleep(0.3)
    sys.exit("No CoreS3 found. Upload the firmware: pio run -d tools/screenshots -t upload")


def read_line(s, until):
    while time.time() < until:
        line = s.readline().decode("ascii", "replace").strip()
        if line:
            return line
    return None


def wait_ready(port, timeout=20):
    """Returns the open port and the screens the firmware offers."""
    until = time.time() + timeout
    while time.time() < until:
        s = open_port(port, until - time.time())
        try:
            while True:
                line = read_line(s, until)
                if line is None:
                    break
                if line.startswith("ready "):
                    return s, line[6:].split(",")
        except serial.SerialException:  # the board restarted
            pass
        s.close()
        time.sleep(0.3)
    sys.exit("The firmware did not answer. Is it the one in tools/screenshots?")


def capture(port, name, out):
    s, _ = wait_ready(port)
    s.reset_input_buffer()
    s.write((name + "\n").encode())
    until = time.time() + 20
    while True:
        line = read_line(s, until)
        if line is None:
            sys.exit(f"{name}: no picture")
        if line.startswith("error "):
            sys.exit(line)
        if line.startswith(f"png {name} "):
            break
    size = int(line.split()[2])
    data = bytearray()
    while len(data) < size and time.time() < until:
        data += s.read(size - len(data))
    s.close()
    if len(data) < size:
        sys.exit(f"{name}: got {len(data)} of {size} bytes")
    path = out / f"{name}.png"
    path.write_bytes(data)
    print(f"{path.relative_to(ROOT) if path.is_relative_to(ROOT) else path}  {size} bytes")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", help="serial port; found by the Espressif USB ID if omitted")
    ap.add_argument("--out", type=pathlib.Path, default=ROOT / "docs" / "screenshots")
    ap.add_argument("screens", nargs="*")
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    s, offered = wait_ready(args.port)
    s.close()
    for name in args.screens or offered:
        if name not in offered:
            sys.exit(f"{name}: not one of {', '.join(offered)}")
        capture(args.port, name, args.out)


if __name__ == "__main__":
    main()
