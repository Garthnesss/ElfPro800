#!/usr/bin/env python3
"""Generate tests/data/chord.mid: 8-note Cmaj stack
(48,55,60,64,67,72,76,79 - one per voice after patch 0002),
120 BPM, PPQ 480, notes on at beat 0, off at beat 4, file ends beat 6."""
import struct

def vlq(n):
    b = [n & 0x7F]
    n >>= 7
    while n:
        b.append((n & 0x7F) | 0x80)
        n >>= 7
    return bytes(reversed(b))

events = b""
events += vlq(0) + b"\xff\x51\x03" + (500000).to_bytes(3, "big")  # tempo
for note in (48, 55, 60, 64, 67, 72, 76, 79):
    events += vlq(0) + bytes([0x90, note, 100])
events += vlq(480 * 4) + bytes([0x80, 48, 0])
for note in (55, 60, 64, 67, 72, 76, 79):
    events += vlq(0) + bytes([0x80, note, 0])
events += vlq(480 * 2) + b"\xff\x2f\x00"

mthd = b"MThd" + struct.pack(">IHHH", 6, 0, 1, 480)
mtrk = b"MTrk" + struct.pack(">I", len(events)) + events
open("tests/data/chord.mid", "wb").write(mthd + mtrk)
print("wrote tests/data/chord.mid")
