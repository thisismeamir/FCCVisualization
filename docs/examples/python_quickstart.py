# Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
"""Documentation example: the quickstart in Python (docs/source/api/python.md).

Run with the build's python directory on PYTHONPATH; CTest runs it as docs::python.
"""
import os
import sys

import fccvis

data = sys.argv[1] if len(sys.argv) > 1 else "data"

# [python]
session = fccvis.Session("quickstart")
print(session.load("csv", os.path.join(data, "hits.csv"), entry="event"))
session.load("csv", os.path.join(data, "tracks.csv"), kind="line", group="track", order="step", entry="event")

event = (fccvis.Scene("event").create()
         .points("hits", "hits").where("energy > 0.5").style("color=gray size=2")
         .style("color=accent size=6", when="energy > 2")
         .lines("tracks", "tracks").style("width=2"))
session.resolve(event)

session.init_backend("raster", "png", output_dir="python-out", width=480, height=480)
session.open("png", "event")
session.flush()                      # python-out/event.png

session.execute("entry event/hits next")   # any shell command works too
session.undo()                             # and so does undo
# [python]

sys.exit(0 if os.path.exists("python-out/event.png") else 1)
