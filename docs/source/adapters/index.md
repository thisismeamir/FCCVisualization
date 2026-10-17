# Adapters

Adapters turn a source (a file, a stream) into series of elements in the
session's data. They never see scenes or backends. Each adapter is optional
(`FCCVIS_ENABLE_<NAME>`) and registers itself by name; `help adapter <name>`
in the shell prints its generated description.

```{toctree}
:maxdepth: 1

csv
detector
edm4hep
phoenix
../reference/adapters
```
