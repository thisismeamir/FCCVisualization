#!/usr/bin/env python3
"""Generates the CSV adapter fixtures in this directory (deterministic).

Run from anywhere: python3 tests/data/csv/make_fixtures.py
The outputs are committed; rerun only when the fixtures must change.
"""
import math
from pathlib import Path

HERE = Path(__file__).resolve().parent


def write(name, text):
    (HERE / name).write_text(text, encoding="utf-8", newline="\n")


# hits.csv: 3 events (written out of order: 2, 0, 1) of calorimeter-like hits
# on a ring of radius 1000 mm; energy has a unit, layer is an integer,
# detector is text, saturated is a flag; one hit has no energy (empty cell).
rows = ["# fixture: calorimeter-like hits, 3 events", "event,x,y,z,energy [GeV],layer,detector,saturated"]
counts = {2: 2, 0: 4, 1: 3}
for event in (2, 0, 1):
    for i in range(counts[event]):
        phi = 2 * math.pi * (i + event / 10) / counts[event]
        energy = "" if (event, i) == (1, 2) else f"{0.5 + i + event:.2f}"
        detector = "ECalBarrel" if i % 2 == 0 else "HCalBarrel"
        saturated = "true" if i == 3 else "false"
        rows.append(f"{event},{1000*math.cos(phi):.3f},{1000*math.sin(phi):.3f},{100*i - 50:.1f},"
                    f"{energy},{i + 1},{detector},{saturated}")
write("hits.csv", "\n".join(rows) + "\n")

# tracks.csv: 2 events of polylines; vertices written in reverse step order
# so the 'order' column matters; charge per track.
rows = ["event;track;step;x;y;z;charge"]
for event in range(2):
    for track in range(2 + event):
        charge = 1 if track % 2 == 0 else -1
        for step in reversed(range(4)):
            r = 200.0 * step
            phi = 0.3 * track + charge * 0.05 * step
            rows.append(f"{event};{track};{step};{r*math.cos(phi):.3f};{r*math.sin(phi):.3f};{10.0*step:.1f};{charge}")
write("tracks.csv", "\n".join(rows) + "\n")

# bad.csv: two malformed rows (wrong field count at line 4, text coordinate
# at line 6) between good ones.
write("bad.csv", "\n".join([
    "x,y,z,id",
    "0,0,0,1",
    "1,1,1,2",
    "2,2,2",
    "3,3,3,4",
    "four,4,4,5",
    "5,5,5,6",
]) + "\n")

# units.csv: coordinates in cm, tab-separated, quoted text with a tab inside.
write("units.csv", "\n".join([
    "x [cm]\ty [cm]\tz [cm]\tlabel",
    "1\t2\t3\t\"a\tb\"",
    "-1.5\t0\t0\tplain",
]) + "\n")
