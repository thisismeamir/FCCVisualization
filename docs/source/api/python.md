# Python

The `fccvis` Python module is a thin binding of the C++ API (pybind11): it
builds changes and calls the session, and adds no behaviour of its own.
Built when `FCCVIS_ENABLE_PYTHON` is on (default when pybind11 and the Python
development files are found and the shared library is built); the module is
`<build>/python/fccvis.*.so` (installed to `lib/fccvis/python`), so put that
directory on `PYTHONPATH`.

```{literalinclude} ../../examples/python_quickstart.py
:language: python
:start-after: "# [python]"
:end-before: "# [python]"
```

## What it offers

| Python | C++ / shell |
|---|---|
| `fccvis.Session(name)` | `session::Session` with a shell interpreter attached |
| `session.load(adapter, path, **options)` | adapter `Open`; returns a `LoadReport` (`ok`, `series`, `skipped`, `str()`) |
| `session.series()` | registered series as dicts (`name`, `source`, `kind`, `entries`, `description`) |
| `fccvis.Scene(name)` | builder: `create()`, `remove()`, `direction(x, y, z)`, `position(...)`, `zoom(m)`, `environment("axes=true")`, `points/lines/surfaces(layer, series, entry=0)`, `layer(kind, name)`, `where(expression)`, `style(assignments, when=...)`, `entry(n)` |
| `fccvis.layout(expression)`, `fccvis.Script()` (`+=`, `describe()`, `len()`) | `api::Script`, `SetLayout` |
| `session.resolve(scene_or_script)` | `Session::Resolve` (one batch); raises `fccvis.Error` when a change fails |
| `session.execute(command)` | one shell command; returns its output lines, raises `fccvis.Error` |
| `init_backend(type, id, **options)`, `open`, `close`, `detach`, `backend_ids`, `flush` | backends |
| `undo()`, `redo()` | undo history |
| `fccvis.adapters()`, `fccvis.backends()` | registered names |

Filter expressions, style assignments and layout expressions use the shell's
syntax (`help filters`, `help styles`, `help layouts`); malformed text raises
`ValueError` with the column of the problem.

Not bound in v1.0: element data access, backend events (picks, log
messages; warnings and errors go to stderr), C++ filters and styles written
as lambdas.
