# Histogram backend

Registry name: `histogram`. Built when `FCCVIS_ENABLE_HISTOGRAM` is on
(default; no dependencies). It does not draw elements: for each open scene it
reduces layers to distributions of **metadata attributes** over the elements
the layer selects. The selection is the layer's filter and current entry, the
same as in every display backend, so a histogram always describes what is on
screen.

```{literalinclude} ../../examples/backend_histogram.cpp
:language: cpp
:start-after: "// [histogram]"
:end-before: "// [histogram]"
```

## Which histograms

Option `histograms`: items `[scene/]layer:attribute[:bins[:lo:hi]]` separated
by `;`. Defaults: every open scene, 20 bins, the data range of the entry (the
maximum included). Without the option, every metadata attribute of every
layer is histogrammed (`unit:*` keys excluded).

Integer, number and flag attributes get fixed-width bins with underflow
(`< lo`) and overflow (`>= hi`) counts; text attributes get one count per value.
Selected elements without the attribute are counted as `missing`. When the
file states a unit (`unit:<attribute>`), it is shown in the SVG title.

## Outputs

| File | Content |
|---|---|
| `<output_dir>/<scene>.hist.csv` | `scene,layer,attribute,kind,bin,low,high,count`; kinds `bin`, `category`, `underflow`, `overflow`, `missing` |
| `<output_dir>/<scene>.hist.svg` | one bar chart per histogram, tiled in `width` x `height` |

Option `formats` (`csv,svg`) selects the outputs. Both are rewritten after
every change that affects the scene (filter, entry, data).

## Not implemented

ROOT `TH1` output and a `HistogramSpec` change in the session's change
catalogue (histograms are configured by backend options only); see STATUS.md.
