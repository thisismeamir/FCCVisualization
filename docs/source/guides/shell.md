# Working in the shell

`fccvis-shell` is the interactive front end: line editing, history,
completion with Tab, and messages from backends printed above the prompt.
The full reference is {doc}`../shell/index`; this page is a short tour.

## A first script

```{literalinclude} ../../examples/guides/shell.fcv
:language: text
:start-after: "# [script]"
:end-before: "# [script]"
```

Typed interactively, the same lines work one by one. A line ending in `\`
continues on the next one (the prompt changes to `..`); indentation of the
continuation is ignored.

## Completion and help

Tab completes commands, scene and layer names, series, adapter and backend
types and their options, filter attributes (read from the data: `where
event/hits e<Tab>` offers `energy`), style keys and presets. `help` lists
the commands; `help filters`, `help styles` and `help layouts` describe the
expression languages; `help adapter csv` and `help backend raster` print what
an adapter or backend supports.

## Saving the state

```{literalinclude} ../../examples/guides/shell.fcv
:language: text
:start-after: "# [save]"
:end-before: "# [save]"
```

`save` writes the commands that rebuild the session (loads, scenes, layers,
filters, styles, layout, cameras) as JSON; `source file.json` replays them.
Backends are not saved.

## Messages

```{literalinclude} ../../examples/guides/shell.fcv
:language: text
:start-after: "# [log]"
:end-before: "# [log]"
```

Warnings and errors are printed as they happen (a backend's render thread
may report later, above the prompt); `log` shows the last session messages,
including the informational ones.

Output:

```{literalinclude} ../../examples/guides/shell.out
:language: text
```

## Options

| Option | Meaning |
|---|---|
| `-f <file>` | run a script, then exit (or continue interactively with `-i`) |
| `-c <command>` | run one command (repeatable) |
| `--keep-going` | do not stop at the first failing command |
| `--reference [section]` | print the generated Markdown reference |
| `--version` | print the version |

History is kept in `~/.fccvis_history` (`FCCVIS_HISTORY` sets another file).
