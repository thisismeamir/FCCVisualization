# Session and Resolve

The session owns the data, the scenes and the layout, and the attached
backends. Every modification is a **change** from one catalogue
(`SceneChanges.h`, `CameraChanges.h`) and goes through `Session::Resolve`,
whether it comes from C++, a script, the shell, a configuration file or a
backend (with the backend's id as origin). Resolve applies the change,
logs it, bumps the revisions of what it touched and synchronises the
backends. A batch (`api::Script`, a shell command with several changes)
stops at the first failing change and notifies backends once.

```{graphviz}
digraph resolve {
  rankdir=LR; bgcolor="transparent";
  node [shape=box, style="rounded,filled", fontname="Helvetica", fontsize=10, color="#525252", fillcolor="#f4f4f4"];
  edge [color="#525252", fontname="Helvetica", fontsize=9];
  change [label="change\n(validate, apply)"];
  log [label="log"]; rev [label="revisions ++"];
  sync [label="sync every backend\nwith its open scenes\nand pruned layout", fillcolor="#e8daff", color="#7017FF"];
  change -> log -> rev -> sync;
}
```

A change validates before it modifies anything, so a failing change leaves
the session unchanged. Backends are attached with `InitBackend(type, id,
options)`, show scenes after `Open(id, scene)` and do their drawing on their
own threads; `Flush(id)` waits until they are done. Messages from backends
(logs, picks, closed windows) reach the session through its port.

## Undo

Because every modification is a change, undo needs no snapshots of the whole
session: before a change is applied, `Change::Inverse` captures the parts it
declares it touches (its footprint: the named layers, the camera, the
layout, a whole scene for creation or removal) and returns a change that puts
them back. `Session::Undo()` resolves the inverses of the last step like any
other batch, so backends redraw exactly what undo changed, and records their
inverses for `Redo()`. Changes from backends (camera moves in a window) are
not recorded. A change that cannot describe its effect returns no inverse and
clears the history. `BeginUndoGroup`/`EndUndoGroup` make several batches one
step (the shell groups each command).
