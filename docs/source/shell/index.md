# The shell

`fccvis-shell` drives a session with commands. Every command that changes
the session goes through `Session::Resolve`, exactly like the C++ API; the
shell adds no other way to modify state. The same interpreter
(`fccvis/api/Interpreter.h`) runs scripts and saved configurations.

```text
fccvis-shell                     interactive
fccvis-shell -f session.fcv      run a script and exit
fccvis-shell -c "scene ls"       run commands and exit (-c may be repeated)
fccvis-shell -i -f setup.fcv     run a script, then continue interactively
```

Non-interactive runs stop at the first failing command and exit with status
1 (`--keep-going` continues, also inside sourced files). In a terminal the
shell edits lines with history (`$FCCVIS_HISTORY`, default
`~/.fccvis_history`; set it empty to disable) and Tab completion; a `\` at
the end of a line continues the command on the next line. When standard
input is not a terminal, lines are read as they come. Relative input paths (`load`, `source`) resolve
against the directory of the script being run; outputs (`save`, backend
`output_dir`) are relative to the working directory.

```{toctree}
:hidden:

config
```

## A session

```{literalinclude} ../../examples/shell_session.fcv
:language: text
:start-after: "# [session]"
:end-before: "# [session]"
```

## Commands

`help` lists every command with its usage; `help <command>` shows one.
`help adapter <name>` and `help backend <type>` print the help generated from
the adapter's or backend's capabilities; `help filters`, `help styles`
(style keys come from the style parser) and `help layouts` describe the
expression languages. All of it is generated from the interpreter's command
table and the registries; `fccvis-shell --reference [commands|adapters|backends]`
prints the same information as Markdown, from which the reference pages of
this documentation are built.

| Command | Purpose |
|---|---|
| `load <adapter> <path> [key=value ...]` | open a source; prints the load report |
| `series [pattern]`, `describe <series>` | list series; show one with its metadata keys |
| `scene new\|rm\|ls\|show` | manage scenes |
| `layer add <scene> <series> [as <layer>] [entry <n>]`, `layer rm\|ls` | manage layers |
| `where <scene>/<layer> <filter>` | select what a layer shows |
| `style <scene>/<layer> [when <filter> then] key=value ...` | add a style rule |
| `style <scene>/<layer> preset <name> [args]` | add a stdlib preset |
| `env <scene> key=value ...`, `env <scene> preset <name>` | background, axes, grid |
| `camera <scene> dir\|pos x y z`, `zoom m`, `reset`, `mode`, `share` | camera |
| `layout <expression>`, `layout show` | layout |
| `entry <scene>/<layer> <n>\|next\|prev\|first\|last` | change event |
| `backend init <type> [id] [key=value ...]`, `backend rm\|ls\|flush` | backends; `init` prints the address a web backend serves (REve page, Phoenix files) |
| `open <backend> <scene> ...`, `close <backend> <scene> ...` | show scenes |
| `undo`, `redo` | undo or redo the last command that changed scenes, layers, cameras or the layout |
| `save <file>`, `source <file>` | configuration files and scripts |
| `log [n]`, `echo`, `help`, `quit` | utilities |

## Undo and redo

`undo` reverts the last command that changed the session's scenes, layers,
cameras or layout, and `redo` applies it again; each prints what it undid
(`undone: where xy/hits energy > 1`). One command is one step, even when it
makes several changes (`scene new a b`, `source script.fcv`). A new command
after `undo` drops the redo history; 100 steps are kept. Not undoable:
loading data (`load`), backends (`backend init`, `open`, `close`), and camera
moves made in an interactive backend window (these are not recorded, and
undoing an earlier command does not reset them). `save` after `undo` writes
the state after the undo.

## Filter expressions

```text
energy > 0.5 and subdetector == "ECal" or inside(box(100, 100, 100))
```

Comparisons `== != < <= > >=` against numbers, `"text"` or `true`/`false`;
`attr in [v, ...]`; `attr ~ "regex"` (whole value); `not`, `and`, `or` (in
that order of precedence) and parentheses. Functions: `all()`, `none()`,
`exists(attr)`, `first(n)`, `index(i)`, `between(attr, lo, hi)`,
`inside(shape[, any|all|centroid])`, `outside(...)`, `within(d, shape[, ...])`.
Shapes in millimetres: `box(dx, dy, dz)` (half-lengths), `sphere(r)`,
`cylinder(rmin, rmax, halfz)`, `cone(rmin1, rmax1, rmin2, rmax2, halfz)`,
`translate(shape, x, y, z)`. A missing attribute fails every comparison.
Errors name the column and mark it:

```text
error: expected a number, a "string" or true/false, found '>' at column 23
  where xy/hits energy >> 1
                        ^
```

## Styles

| Kind | Keys |
|---|---|
| point | `color`, `marker` (circle, square, diamond, triangle, cross), `opacity`, `size`, `visible` |
| line | `color`, `dash` (solid, dashed, dotted, dashdot), `opacity`, `visible`, `width` |
| surface | `color`, `edgecolor`, `edgewidth`, `opacity`, `visible`, `wireframe` |
| environment | `axes`, `background`, `grid` |

Colours: `#rgb`, `#rrggbb`, `#rrggbbaa` or a name (`accent`, `black`, `blue`,
`cyan`, `gray`, `green`, `magenta`, `orange`, `purple`, `red`, `white`,
`yellow`). Presets: `hits-by-energy [lo hi [attribute]]`,
`tracks-by-charge [attribute]`, `detector-outline`, `ghosted`,
`color-by <attribute> <lo> <hi> [bins]`,
`size-by <attribute> <min> <max> <lo> <hi> [bins]`; environment presets
`publication`, `default`.

## Layout expressions

`xy`, `split(a, b)`, `vsplit(a, b)`, `tabs(a, b)`, nested freely; a child may
carry `as "label"` and, in splits, `*weight`:
`split(xy as "Front" *2, tabs(rz, xz))`.

## Completion

Press Tab in the interactive shell. Candidates come from the live session
and the registries, sorted: command and subcommand names; scene names,
`scene/layer` paths, series names and backend ids; adapter names and their
options (from the adapter's capabilities, as `key=`); backend types and their
options; in filters, the metadata keys of the layer's series (entry 0,
without `unit:*`), operators and functions; in styles, the keys allowed for
the layer's element kind, `when`/`then` and the presets that fit the kind;
help topics; file names for `load`, `source` and `save`. The engine is
`Interpreter::Complete(line)`, usable from any front end.

## Messages

Errors and warnings of the command you typed are printed with its output.
Messages that backends emit later from their own threads (render failures,
closed windows) are printed when they happen.
