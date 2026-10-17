# Shapes

Header: `fccvis/stdlib/Shapes.h` (module `stdlib`). Every shape implements
`geometry::AbstractShape`:

- `Criterion(p)`: approximate signed distance in millimetres, `<= 0` inside;
- `Tessellate(tolerance)`: closed, outward-oriented triangle mesh (relative
  tolerance: chord sagitta / radius, or cell size / bounding-box diagonal for
  boolean shapes);
- `Clone()`.

| Shape | Parameters |
|---|---|
| `Box` | half-lengths `dx, dy, dz` |
| `Sphere` | radius |
| `Cylinder`, `TubeSegment` | `rMin, rMax, halfZ [, phiStart, phiDelta]` (full, hollow or phi section) |
| `Cone` | `rMin1, rMax1, rMin2, rMax2, halfZ [, phiStart, phiDelta]` (frustum) |
| `Polycone` | z planes with `rMin`, `rMax` per plane `[, phiStart, phiDelta]` |
| `HalfSpace` | `normal, offset [, extent]`: points with `n·p <= offset` (infinite; drawn as a square patch) |
| `Wedge` | `phiStart, phiDelta, thetaMin, thetaMax, rMax` (spherical sector) |
| `Union`, `Intersection`, `Difference` | two shapes; membership from the operands' criteria |
| `Transformed` | a shape and a rigid `geometry::Transform` |

Boolean shapes are tessellated by marching tetrahedra on a regular grid over
their bounding box (Kuhn subdivision, so the mesh is watertight); edges are
rounded at the scale of one cell. All other shapes are tessellated exactly
from their profiles.

```{literalinclude} ../../examples/stdlib_shapes.cpp
:language: cpp
:start-after: "// [shapes]"
:end-before: "// [shapes]"
:dedent: 2
```

Helpers: `BoundsOf(shape)`, `BoundsOf(mesh)`, `IsClosed(mesh)`,
`SignedVolume(mesh)`, `SegmentsForTolerance(tolerance)`, `MarchTetrahedra`.
