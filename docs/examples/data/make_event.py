#!/usr/bin/env python3
# Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
"""Generates the example event data used by the documentation (deterministic).

events.csv: calorimeter-like hits on barrel layers around jets of charged
tracks; tracks.csv: helix-like polylines from the origin in a 2 T field
(the bending is illustrative, not physics). Three events. Run:
    python3 docs/examples/data/make_event.py
"""
import math
import random
from pathlib import Path

HERE = Path(__file__).resolve().parent
rng = random.Random(20261011)

hit_rows = ["event,x [mm],y [mm],z [mm],energy [GeV],layer,subdetector,cell"]
track_rows = ["event,track,step,x,y,z,charge,pt [GeV]"]
for event in range(3):
    cell = 0
    jets = [(rng.uniform(0, 2 * math.pi), rng.uniform(-0.6, 0.6)) for _ in range(2 + event % 2)]
    track_id = 0
    for phi0, eta in jets:
        for _ in range(rng.randint(4, 7)):
            charge = rng.choice([-1, 1])
            pt = rng.uniform(1.0, 20.0)
            radius = pt / (0.3 * 2.0) * 1000.0  # mm, illustrative
            phi = phi0 + rng.gauss(0, 0.15)
            dz = math.sinh(eta + rng.gauss(0, 0.1))
            for step in range(0, 21):
                s = step * 75.0  # mm along the transverse arc
                ang = phi - charge * s / (2 * radius)
                track_rows.append("%d,%d,%d,%.2f,%.2f,%.2f,%d,%.2f" % (
                    event, track_id, step, s * math.cos(ang), s * math.sin(ang), s * dz, charge, pt))
            track_id += 1
        # calorimeter showers around the jet direction
        for layer in range(1, 9):
            r = 1500.0 + 60.0 * layer
            det = "ECalBarrel" if layer <= 5 else "HCalBarrel"
            for _ in range(rng.randint(6, 14)):
                p = phi0 + rng.gauss(0, 0.06 + 0.01 * layer)
                z = r * math.sinh(eta + rng.gauss(0, 0.05))
                e = max(0.01, rng.expovariate(1.0 / (3.0 if det == "ECalBarrel" else 1.5)) / layer)
                hit_rows.append("%d,%.1f,%.1f,%.1f,%.3f,%d,%s,%d" % (
                    event, r * math.cos(p), r * math.sin(p), z, e, layer, det, cell))
                cell += 1
    # noise hits
    for _ in range(25):
        p = rng.uniform(0, 2 * math.pi)
        layer = rng.randint(1, 8)
        r = 1500.0 + 60.0 * layer
        hit_rows.append("%d,%.1f,%.1f,%.1f,%.3f,%d,%s,%d" % (
            event, r * math.cos(p), r * math.sin(p), rng.uniform(-2000, 2000), rng.uniform(0.01, 0.08), layer,
            "ECalBarrel" if layer <= 5 else "HCalBarrel", cell))
        cell += 1

(HERE / "hits.csv").write_text("\n".join(hit_rows) + "\n", encoding="utf-8")
(HERE / "tracks.csv").write_text("\n".join(track_rows) + "\n", encoding="utf-8")
print("hits:", len(hit_rows) - 1, "tracks rows:", len(track_rows) - 1)
