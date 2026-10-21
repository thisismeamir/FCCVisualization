# Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
"""T5.8: Python binding (module fccvis). Run by CTest as python::binding."""
import os
import sys
import tempfile
import unittest

import fccvis

DATA = os.path.join(os.path.dirname(__file__), "..", "data", "csv")


class Binding(unittest.TestCase):
    def setUp(self):
        self.s = fccvis.Session("test")
        report = self.s.load("csv", os.path.join(DATA, "hits.csv"), entry="event")
        self.assertTrue(report.ok, str(report))
        self.assertEqual(report.series, ["hits"])

    def test_registries_and_series(self):
        self.assertIn("csv", fccvis.adapters())
        self.assertIn("raster", fccvis.backends())
        series = self.s.series()
        self.assertEqual(len(series), 1)
        self.assertEqual(series[0]["name"], "hits")
        self.assertEqual(series[0]["kind"], "point")
        self.assertGreater(series[0]["entries"], 1)
        self.assertTrue(fccvis.__version__)

    def test_builders_resolve_and_backend(self):
        scene = (fccvis.Scene("xy").create().direction(0, 0, -1).zoom(2)
                 .points("hits", "hits").where("energy > 1").style("color=red size=5")
                 .style("marker=square", when="layer >= 3"))
        self.assertEqual(scene.script.describe()[0], "CreateScene(xy)")
        result = self.s.resolve(scene)
        self.assertTrue(result.ok)
        self.assertEqual(result.applied, len(scene.script))
        script = fccvis.Script()
        script += fccvis.Scene("rz").create().direction(1, 0, 0).points("hits", "hits")
        script += fccvis.layout('split(xy as "Front", rz)')
        self.assertTrue(self.s.resolve(script))
        self.assertEqual(sorted(self.s.scene_names()), ["rz", "xy"])

        out = tempfile.mkdtemp()
        self.s.init_backend("raster", "png", output_dir=out, width=160, height=120)
        self.s.open("png", "xy")
        self.s.open("png", "rz")
        self.assertTrue(self.s.flush())
        self.assertEqual(sorted(os.listdir(out)), ["layout.png", "rz.png", "xy.png"])
        self.assertEqual(self.s.backend_ids(), ["png"])
        self.s.detach("png")
        self.assertEqual(self.s.backend_ids(), [])

    def test_shell_commands_and_undo(self):
        self.s.execute("scene new a")
        self.s.execute("layer add a hits")
        self.assertIn("  layer hits (point): series hits, entry 0", self.s.execute("scene show a"))
        self.s.execute("camera a zoom 3")
        self.assertTrue(self.s.undo())
        self.assertIn("  camera: dir 0 0 1, pos 0 0 0, zoom 1, mode free", self.s.execute("scene show a"))
        self.assertTrue(self.s.redo())
        self.assertIn("  camera: dir 0 0 1, pos 0 0 0, zoom 3, mode free", self.s.execute("scene show a"))

    def test_errors(self):
        with self.assertRaises(fccvis.Error):
            self.s.execute("scene rm nope")
        with self.assertRaises(fccvis.Error):
            self.s.resolve(fccvis.Scene("x").create().create())  # second create fails
        with self.assertRaises(ValueError):
            fccvis.Scene("x").create().points("p", "hits").where("energy >")
        with self.assertRaises(ValueError):
            fccvis.Scene("x").where("energy > 1")  # no layer selected
        with self.assertRaises(ValueError):
            self.s.load("nope", "file")
        self.assertFalse(self.s.load("csv", "/nonexistent.csv").ok)
        self.assertTrue(issubclass(fccvis.Error, RuntimeError))


if __name__ == "__main__":
    unittest.main(verbosity=2)
