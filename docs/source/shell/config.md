# Configuration files

`save <file>` writes the commands that rebuild the session; `source <file>`
(or `fccvis-shell -f <file>`) replays them through the interpreter, so every
change goes through `Session::Resolve` again. In C++ the same format is read
and written by `fccvis/api/Config.h` (`WriteConfig`, `ReadConfig`).

```{literalinclude} ../../examples/session_config.json
:language: json
```

What is saved:

- the `load` commands, with adapter options; paths are written absolute
  (relative paths in a hand-written file resolve against the file's
  directory). The data itself is not stored;
- the scene, layer, filter, style, environment, entry and layout commands, in
  the order they were run (removed scenes are created and removed again);
- the final camera of every scene (`dir`, `pos`, `zoom`), which also captures
  moves made in a viewer. Camera `mode` and `share` commands are kept as run.

What is not saved: backends and open scenes (they belong to the running
program), and changes made from C++ with filters or styles that have no text
form (lambdas).

Loading checks the header: `format` must be `fccvis-config` and `version`
not newer than the reader (currently 1). Malformed JSON is reported with line
and column. Replay stops at the first failing command and names it.
