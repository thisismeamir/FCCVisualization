# Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
# Configuration file for the Sphinx documentation builder.
#
# Build through CMake (target `docs`, FCCVIS_BUILD_DOCS=ON), which runs
# Doxygen first and passes FCCVIS_DOXYGEN_XML and FCCVIS_VERSION. Warnings
# are errors (-W). API pages are generated from the Doxygen XML (one page per
# module) at the start of the build.
import os
import re
import xml.etree.ElementTree as ET
from pathlib import Path

project = 'fccvis'
copyright = '2026, FCC Project'
author = 'Amir H. Ebrahimnezhad'
release = os.environ.get("FCCVIS_VERSION", "1.0.0")
version = release

extensions = [
    "myst_parser",
    "breathe",
    "sphinx.ext.graphviz",
    "sphinxcontrib.mermaid",
]

html_theme = "alabaster"
html_theme_options = {"fixed_sidebar": True, "page_width": "1100px", "sidebar_width": "250px"}
html_sidebars = {"**": ["about.html", "navigation.html", "relations.html", "searchbox.html"]}
graphviz_output_format = "svg"
source_suffix = {
    ".rst": "restructuredtext",
    ".md": "markdown",
}
exclude_patterns = ["_build", "reference/generated/*"]  # generated pages are included, not built alone

myst_enable_extensions = [
    "colon_fence",
    "deflist",
]
myst_heading_anchors = 3

_xml = os.environ.get("FCCVIS_DOXYGEN_XML", str(Path(__file__).resolve().parents[1] / "doxygen" / "xml"))
breathe_projects = {"fccvis": _xml}
breathe_default_project = "fccvis"
breathe_default_members = ("members",)
cpp_index_common_prefix = ["fccvis::"]


def _generate_api_pages(app):
    """One page per module (directory of src/<module>/include), listing its headers.

    Each header gets its description and namespace-scope members (doxygenfile
    restricted to those sections) and then each class it defines
    (doxygenclass/doxygenstruct). Namespaces are not rendered per file: they
    span files and would be declared several times.
    """
    out = Path(app.srcdir) / "api" / "generated"
    out.mkdir(parents=True, exist_ok=True)
    index = Path(_xml) / "index.xml"
    modules = {}
    if index.exists():
        for compound in ET.parse(index).getroot().findall("compound"):
            if compound.get("kind") != "file":
                continue
            refid = compound.get("refid")
            detail = ET.parse(Path(_xml) / (refid + ".xml")).getroot().find("compounddef")
            location = detail.find("location")
            path = location.get("file") if location is not None else compound.findtext("name")
            parts = Path(path).parts
            module = parts[0] if parts and parts[0] not in ("include", "fccvis") else "config"
            classes = []
            for inner in detail.findall("innerclass"):
                cref = inner.get("refid")
                cdef = ET.parse(Path(_xml) / (cref + ".xml")).getroot().find("compounddef")
                name = cdef.findtext("compoundname")
                if "::detail::" in name or "<" in name:
                    continue  # implementation details; explicit specializations are shown with their primary template
                classes.append((cdef.get("kind"), name))
            names = {n for _, n in classes}
            # nested classes are rendered inside their enclosing class
            classes = [(k, n) for k, n in classes if not any(n.startswith(o + "::") for o in names if o != n)]
            modules.setdefault(module, []).append((path, classes))
    for module, files in sorted(modules.items()):
        lines = ["# Module `%s`\n" % module, ""]
        for f, classes in sorted(files):
            lines += ["## %s\n" % Path(f).name, "", "```{doxygenfile} %s" % Path(f).name, ":project: fccvis",
                      ":sections: briefdescription detaileddescription func typedef enum var define",
                      "```", ""]
            for kind, name in classes:
                directive = "doxygenstruct" if kind == "struct" else "doxygenclass"
                lines += ["```{%s} %s" % (directive, name), ":project: fccvis", ":members:", "```", ""]
        page = out / ("%s.md" % module)
        text = "\n".join(lines)
        if not page.exists() or page.read_text() != text:
            page.write_text(text)
    for stale in out.glob("*.md"):
        if stale.stem not in modules:
            stale.unlink()


def setup(app):
    app.connect("builder-inited", _generate_api_pages)
