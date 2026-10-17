# Backends

Backends draw the scenes a session holds. Each is optional
(`FCCVIS_ENABLE_<NAME>`), registers itself by name, declares its
capabilities (element kinds, style properties, layout support, options) and
passes the same contract and matrix tests.

```{toctree}
:maxdepth: 1

raster
root
reve
svg
histogram
ced
phoenix
../reference/backends
```
