# Scripting

There are three ways to drive a session without typing: shell scripts
(`fccvis-shell -f`, `source`), C++ scripts of changes, and the command
interpreter called from C++. All end in the same place: changes resolved by
the session.

## Shell scripts

A script is a file of shell commands, one per line (`\` continues a line,
`#` starts a comment). `fccvis-shell -f script.fcv` runs it and stops at the
first failing command with exit status 1 (`--keep-going` continues);
`source script.fcv` runs one from the shell. Relative input paths in a
script are relative to the script's directory; outputs are relative to the
working directory. `save file.json` writes the commands that rebuild the
current state, which `source` replays: see {doc}`../shell/config`.

## C++ scripts of changes

```{literalinclude} ../../examples/guide_scripting.cpp
:language: cpp
:start-after: "// [script]"
:end-before: "// [script]"
```

A `Script` is an ordered list of immutable changes: it can be kept, copied,
extended and run on several sessions. `Run` resolves it as one batch:

```{literalinclude} ../../examples/guide_scripting.cpp
:language: cpp
:start-after: "// [batch]"
:end-before: "// [batch]"
```

prints `ok=0 applied=1: CreateScene: scene 'front' already exists`: the zoom
before the failing change was applied, the one after it was not.

## The interpreter from C++

```{literalinclude} ../../examples/guide_scripting.cpp
:language: cpp
:start-after: "// [interpreter]"
:end-before: "// [interpreter]"
```

`Interpreter::Execute` runs one command and `Run` a whole script; the
`Reply` holds the output lines and errors instead of printing them.

## Replaying a configuration

```{literalinclude} ../../examples/guide_scripting.cpp
:language: cpp
:start-after: "// [config]"
:end-before: "// [config]"
```

`StateCommands()` are the commands that rebuild what the interpreter did,
with the final camera of every scene (including moves made in an
interactive backend). The example checks that both sessions show the same
`front` scene.

## Undo

```{literalinclude} ../../examples/guide_scripting.cpp
:language: cpp
:start-after: "// [undo]"
:end-before: "// [undo]"
```

See {doc}`../concepts/session` for how inverses are computed, and the shell's
`undo`/`redo` in {doc}`../shell/index`.
