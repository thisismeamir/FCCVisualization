# Reading EDM4hep

The EDM4hep adapter registers one series per collection of an EDM4hep
(podio) event file; an entry is one event. Reference:
{doc}`../adapters/edm4hep`.

## Load selected collections

```{literalinclude} ../../examples/guides/edm4hep.fcv
:language: text
:start-after: "# [load]"
:end-before: "# [load]"
```

`collections=` takes name patterns, so large files register only what you
need. The metadata names are the EDM4hep member names (`energy`, `EDep`,
`PDG`, `generatorStatus`, ...).

## Select and colour

```{literalinclude} ../../examples/guides/edm4hep.fcv
:language: text
:start-after: "# [select]"
:end-before: "# [select]"
```

## Follow a relation

Relations are not joined, but a single relation is stored as the related
object's index (`particle` and `particle_collection` for a simulated
tracker hit). Showing the hits of MC particle 0:

```{literalinclude} ../../examples/guides/edm4hep.fcv
:language: text
:start-after: "# [relations]"
:end-before: "# [relations]"
```

## Step through events

Each layer has its own entry, so a layer can show another event than its
neighbours (for example to compare two events in one view):

```{literalinclude} ../../examples/guides/edm4hep.fcv
:language: text
:start-after: "# [events]"
:end-before: "# [events]"
```

## Histograms of the selection

The histogram backend fills distributions from exactly what the layers
select (same filters, same entry):

```{literalinclude} ../../examples/guides/edm4hep.fcv
:language: text
:start-after: "# [histogram]"
:end-before: "# [histogram]"
```

Output:

```{literalinclude} ../../examples/guides/edm4hep.out
:language: text
```
