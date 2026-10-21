#!/usr/bin/env python3
# Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
"""Checks a Phoenix event-data JSON file against the format documented by
Phoenix (guides/developers/event_data_format.md and
packages/phoenix-event-display/src/loaders/objects/phoenix-objects.ts at
commit c770587, 2026-10-09): only the parts fccvis writes (event keys,
'event number'/'run number', Hits objects, Tracks). Usage: phoenix_check.py FILE"""
import json
import sys


def fail(msg):
    print("FAIL:", msg)
    sys.exit(1)


def number(x, what):
    if not isinstance(x, (int, float)) or isinstance(x, bool):
        fail("%s: expected a number, got %r" % (what, x))


with open(sys.argv[1]) as f:
    events = json.load(f)
if not isinstance(events, dict) or not events:
    fail("top level must be a non-empty object of events")
for key, event in events.items():
    if not isinstance(event, dict):
        fail("event %r must be an object" % key)
    for meta in ("event number", "run number"):
        if meta in event:
            number(event[meta], "%s/%s" % (key, meta))
    for otype, collections in event.items():
        if otype in ("event number", "run number"):
            continue
        if otype not in ("Hits", "Tracks"):
            fail("%s: unexpected object type %r" % (key, otype))
        if not isinstance(collections, dict):
            fail("%s/%s must map collection names to lists" % (key, otype))
        for name, objects in collections.items():
            where = "%s/%s/%s" % (key, otype, name)
            if not isinstance(objects, list):
                fail(where + " must be a list")
            first_type = None
            for i, o in enumerate(objects):
                w = "%s[%d]" % (where, i)
                if not isinstance(o, dict):
                    fail(w + " must be an object")
                if "color" in o and not isinstance(o["color"], str):
                    fail(w + ".color must be a string")
                if otype == "Hits":
                    t = o.get("type", "Point")
                    if t not in ("Point", "CircularPoint", "Line", "Box"):
                        fail(w + ": bad hit type %r" % t)
                    first_type = first_type or t
                    if t != first_type:
                        fail(where + ": all hits of a collection must have the same type")
                    n = 3 if t in ("Point", "CircularPoint") else 6
                    if not isinstance(o.get("pos"), list) or len(o["pos"]) != n:
                        fail(w + ".pos must have %d numbers" % n)
                    for c in o["pos"]:
                        number(c, w + ".pos")
                else:
                    pos = o.get("pos")
                    if not isinstance(pos, list) or len(pos) < 2:
                        fail(w + ".pos must list at least two [x, y, z] positions")
                    for p in pos:
                        if not isinstance(p, list) or len(p) != 3:
                            fail(w + ".pos entries must be [x, y, z]")
                        for c in p:
                            number(c, w + ".pos")
                    if "linewidth" in o:
                        number(o["linewidth"], w + ".linewidth")
print("ok:", sys.argv[1])
