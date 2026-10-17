# Revisions

Backends do not observe the session. Each change bumps revision counters of
what it touched: per scene the camera, camera options, environment, the set
of layers and each layer's content; the layout; the data. At every sync a
backend compares the counters with those it drew last (`backend::RevisionTracker`)
and redraws only what moved:

- a scene opened since the last sync is drawn completely;
- otherwise only the parts and layers whose revisions moved are redrawn;
- a data change (series registered or replaced) redraws every layer; a live
  series that grew redraws only the layers showing it;
- when the change came from the backend itself (origin), it is already on its
  screen: revisions are recorded and nothing is redrawn;
- the layout is reapplied when its revision or the set of open scenes moved.

```{graphviz}
digraph revisions {
  rankdir=LR; bgcolor="transparent";
  node [shape=record, fontname="Helvetica", fontsize=10, color="#525252", style=filled, fillcolor="#f4f4f4"];
  edge [color="#525252", fontname="Helvetica", fontsize=9];
  now [label="{session|camera 7|layer hits 12|layer trk 5}"];
  drawn [label="{last drawn|camera 7|layer hits 11|layer trk 5}"];
  plan [label="{plan|redraw layer hits}", fillcolor="#e8daff", color="#7017FF"];
  now -> plan; drawn -> plan;
}
```

The contract and matrix tests check this with the backends' draw counters
(`DrawStats`): a filter change redraws one layer, a camera change only the
camera.
