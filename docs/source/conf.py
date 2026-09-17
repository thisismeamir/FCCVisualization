# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = 'fccvis'
copyright = '2026, FCC Project'
author = 'Amir H. Ebrahimnezhad'
release = '0.0.1'

extensions = [
    "myst_parser",
    "breathe",
    "sphinx.ext.graphviz",
    "sphinxcontrib.mermaid",
]

html_theme = "alabaster"
graphviz_output_format = "svg"
source_suffix = {
    ".rst": "restructuredtext",
    ".md": "markdown",
}

myst_enable_extensions = [
    "colon_fence",
    "deflist",
]

breathe_projects = {
    "fccvis" : "../doxygen/xml"
}

breathe_default_project = "fccvis"
