
# FCCVisualization

A modular and extensible event-visualisation and monitoring framework for the **Future Circular Collider (FCC)** software ecosystem, designed around PandoraSDK and ROOT/TEve.

FCCVisualization refactors and generalises the existing Pandora monitoring infrastructure into a persistent, scene-based visualisation system while maintaining compatibility with the established PandoraSDK monitoring interface.

---

## Project Status

**Status:** Early development / architectural refactoring

FCCVisualization is currently being developed as a successor and generalisation of the existing `PandoraMonitoring` implementation used within the Pandora software ecosystem.

The architecture is being designed before the implementation is stabilised. APIs and internal structures may therefore change during early development.

---

## Goals

FCCVisualization has four primary goals.

### 1. Generalise Pandora visualisation

The existing Pandora monitoring implementation contains substantial detector-, view-, and event-display-specific logic. FCCVisualization aims to replace this hardcoded structure with a general visualisation model.

The framework should support:

* dynamically defined scenes;
* configurable cameras and projections;
* configurable detector geometry;
* reusable visualisation configurations;
* configurable colours and rendering properties;
* spatial slicing;
* general object filtering;
* multiple simultaneous views;
* event navigation;
* interactive inspection of events.

The system should make it possible to add new visualisation concepts without modifying a central collection of hardcoded viewers and scenes.

---

### 2. Maintain PandoraSDK compatibility

FCCVisualization is intended to work with **PandoraSDK** and must therefore treat the existing Pandora monitoring API as a compatibility boundary.

Existing interfaces such as:

```cpp
PandoraMonitoringApi::VisualizeTracks(...)
PandoraMonitoringApi::VisualizeClusters(...)
PandoraMonitoringApi::VisualizeCaloHits(...)
PandoraMonitoringApi::VisualizeParticleFlowObjects(...)
PandoraMonitoringApi::SetSlice(...)
```

should remain usable where compatibility requires them.

The implementation behind these interfaces may be substantially redesigned.

The guiding principle is:

> **Change the implementation freely; change the Pandora-facing contract conservatively.**

This allows Pandora algorithms and applications to continue using familiar monitoring calls while gaining access to the new visualisation infrastructure.

---

## 3. Introduce persistent monitoring sessions

FCCVisualization should not treat event visualisation as a one-shot operation.

Instead, it provides a persistent monitoring session containing resources that can survive across events.

Conceptually:

```text
Application
    │
    └── Session
          ├── Geometry
          ├── Scene Manager
          ├── Scenes
          ├── Filters
          └── Event State
```

Expensive resources such as the ROOT application, TEve manager, detector geometry, viewers, and scene configuration should not need to be reconstructed for every event.

The event itself becomes data evaluated by the existing visualisation configuration.

---

## 4. Provide an interactive monitoring application

FCCVisualization is intended to operate as a ROOT-based graphical application rather than exclusively as a programmatic event display.

The application should eventually allow users to:

* open ROOT data files;
* inspect available event data;
* navigate between events;
* create and remove scenes;
* configure cameras;
* configure detector geometry;
* enable or disable visualised objects;
* define filters;
* configure colours;
* modify rendering parameters;
* save and restore visualisation sessions.

The same underlying scene and filtering model should be usable both programmatically and through the GUI.

---

# Architecture

FCCVisualization is organised around a separation between the **visualisation model**, **rendering backend**, **Pandora compatibility layer**, and **user interface**.

The intended architecture is:

```text
                         PandoraSDK
                             │
                             ▼
                ┌────────────────────────┐
                │ PandoraMonitoringApi   │
                │ Compatibility Layer    │
                └────────────┬───────────┘
                             │
                             ▼
                ┌────────────────────────┐
                │     Monitoring Core    │
                │                        │
                │ Session                │
                │ Event                  │
                │ Resource Management    │
                └────────────┬───────────┘
                             │
              ┌──────────────┼──────────────┐
              │              │              │
              ▼              ▼              ▼
          Scene System   Filter System   Geometry
              │
              ▼
       Visualisation Model
              │
        ┌─────┴─────┐
        │           │
        ▼           ▼
     Renderer      GUI
        │           │
        ▼           ▼
      ROOT/TEve   ROOT GUI
```

The central design principle is that **ROOT/TEve should be a rendering implementation, not the definition of the visualisation model**.

This makes the core architecture independent of the details of the current rendering backend.

---

# Core Concepts

## Monitoring

The monitoring system manages the lifetime of the visualisation environment.

It is responsible for:

* application lifecycle;
* monitoring sessions;
* event lifecycle;
* global resources;
* access to scenes;
* access to rendering backends.

---

## Session

A session represents the current state of a visualisation application.

A session may contain:

```text
Session
├── Data Source
├── Geometry
├── Scene Manager
├── Scene Configuration
├── Filter Configuration
└── Event State
```

Sessions should eventually be serialisable so that a user can save a visualisation configuration and restore it later.

---

## Event

An event contains the data currently being visualised.

Depending on the data source, this may include:

* MC particles;
* tracks;
* calorimeter hits;
* clusters;
* particle-flow objects;
* vertices;
* user-defined visualisation objects.

The event is transient relative to the session.

Changing the event should not require reconstructing the scene configuration.

---

## Scene

A scene is a persistent description of a particular visualisation.

A scene may define:

```text
Scene
├── Camera
├── Geometry
├── Visibility
├── Filters
└── Rendering Configuration
```

Examples include:

```text
3D
XY
XZ
YZ
Central Detector
Tracking
Calorimeter
High Energy Objects
Muon View
```

The standard FCC/Pandora views should therefore be **default scene configurations**, rather than hardcoded structural components of the monitoring engine.

Users should be able to create arbitrary additional scenes.

---

## Camera

A camera describes how a scene is presented.

The camera model should support concepts such as:

* perspective projection;
* orthographic projection;
* detector-axis orientation;
* arbitrary camera position;
* target position;
* orientation;
* zoom.

The common XY/XZ/YZ views should be predefined configurations, not special cases in the rendering implementation.

---

## Geometry

Detector geometry is a persistent resource.

FCCVisualization should separate detector geometry from event content so that the same geometry can be shared by multiple scenes.

Conceptually:

```text
                Geometry
               /    |    \
              /     |     \
            3D      XY     XZ
          Scene    Scene   Scene
```

This prevents the geometry from being reconstructed independently for every view.

---

## Filters

Filtering is a first-class part of the visualisation model.

A filter determines which objects are eligible for visualisation.

Spatial filters may include:

```text
x ∈ [xmin, xmax]
y ∈ [ymin, ymax]
z ∈ [zmin, zmax]
```

or more general regions such as:

```text
inside box
inside cylinder
inside detector volume
```

Property-based filters may eventually include:

```text
energy > threshold
momentum > threshold
PDG == 13
charge != 0
```

Filters should also support composition:

```text
Energy > 1 GeV
        AND
Inside central detector
```

The existing Pandora `VisualizationSlice` functionality should be treated as an important compatibility concept within this more general filtering system.

---

## Visualisation Objects

FCCVisualization should provide representations for Pandora data types without making the entire visualisation architecture dependent on individual object implementations.

Initial supported concepts include:

```text
MC particles
Tracks
Calorimeter hits
Clusters
Particle-flow objects
Vertices
Markers
Lines
```

Each representation should be responsible for converting domain data into a renderable representation.

---

# Interactive GUI

The GUI is a frontend to the monitoring model rather than the owner of it.

The intended application will provide interfaces conceptually similar to:

```text
+-----------------------------------------------------------+
| File   Event   Scene   View   Filters   Settings          |
+----------------------+------------------------------------+
| Scenes               |                                    |
|                      |                                    |
| 3D                   |                                    |
| XY                   |            Event Display           |
| XZ                   |                                    |
| YZ                   |                                    |
| Central Detector     |                                    |
|                      |                                    |
+----------------------+------------------------------------+
| Object / Filter      | Scene Properties                   |
| Configuration        | Camera / Rendering / Geometry     |
+-----------------------------------------------------------+
```

The GUI should manipulate the same objects exposed through the programmatic API.

For example, creating a scene through the GUI and creating one through C++ should result in the same underlying scene representation.

---

# Data Sources

FCCVisualization is intended to support multiple sources of event data.

The first target is:

```text
PandoraSDK
```

and offline ROOT data.

Conceptually:

```text
Pandora
    │
    ▼
Monitoring Session
```

and:

```text
ROOT File
    │
    ▼
Monitoring Session
```

should eventually provide the same visualisation interface.

This allows the framework to be used both during Pandora processing and as an independent event-inspection application.

---

# Online and Offline Operation

FCCVisualization should support two principal modes.

## Pandora-integrated mode

Pandora creates or accesses the monitoring infrastructure during event processing.

```text
PandoraSDK
    │
    ▼
PandoraMonitoringApi
    │
    ▼
FCCVisualization
    │
    ▼
Interactive / Headless Output
```

This mode must preserve the existing Pandora monitoring semantics.

## Offline mode

The application opens previously produced data.

```text
ROOT File
    │
    ▼
FCCVisualization
    │
    ▼
Interactive Event Browser
```

The same scenes, filters, cameras, colours, and rendering configurations should be usable in both modes.

---

# Headless Operation

The GUI must not be a mandatory dependency for batch processing.

FCCVisualization should therefore distinguish between:

```text
Interactive
```

and:

```text
Headless
```

operation.

Headless operation should eventually support use cases such as:

* automated event rendering;
* batch visualisation;
* debugging;
* image generation;
* CI/testing;
* production jobs.

The visualisation model should remain identical between interactive and headless operation.

---

# Compatibility Strategy

Compatibility with PandoraSDK is a core project requirement.

The compatibility strategy is:

### Preserve

* established public class names where required;
* established API names;
* established method semantics;
* Pandora data types;
* existing monitoring workflows.

### Refactor

* scene creation;
* viewer management;
* event-display lifecycle;
* geometry management;
* filtering;
* rendering configuration;
* ROOT/TEve implementation.

### Generalise

* scenes;
* cameras;
* filters;
* visualisation objects;
* data sources;
* session management.

The existing `PandoraMonitoringApi` should therefore become a **compatibility façade over the new engine**, rather than the architecture around which the new engine is built.

---

# Intended Repository Structure

The initial repository is organised into conceptual layers:

```text
FCCVisualization/
│
├── CMakeLists.txt
├── LICENSE
├── README.md
├── CHANGELOG.md
│
├── cmake/
│
├── include/
│   └── PandoraMonitoringApi.h
│
├── src/
│   ├── api/
│   │   └── PandoraMonitoringApi.cc
│   │
│   ├── core/
│   │   ├── Monitoring.cc
│   │   ├── Session.cc
│   │   └── Event.cc
│   │
│   ├── scene/
│   │   ├── Scene.cc
│   │   ├── SceneManager.cc
│   │   └── Camera.cc
│   │
│   ├── filter/
│   │   ├── Filter.cc
│   │   ├── SpatialFilter.cc
│   │   └── CompositeFilter.cc
│   │
│   ├── geometry/
│   │   └── Geometry.cc
│   │
│   ├── visualization/
│   │   ├── TrackVisualization.cc
│   │   ├── CaloHitVisualization.cc
│   │   ├── ClusterVisualization.cc
│   │   ├── PfoVisualization.cc
│   │   ├── MCParticleVisualization.cc
│   │   └── ...
│   │
│   ├── backend/
│   │   └── root/
│   │       ├── RootApplication.cc
│   │       ├── RootRenderer.cc
│   │       ├── EveScene.cc
│   │       └── EveViewer.cc
│   │
│   └── gui/
│       ├── MonitoringApplication.cc
│       ├── MainWindow.cc
│       ├── ScenePanel.cc
│       ├── FilterPanel.cc
│       └── PropertiesPanel.cc
│
├── tests/
│   ├── core/
│   ├── scene/
│   ├── filter/
│   ├── visualization/
│   └── api/
│
├── examples/
│   ├── basic/
│   └── interactive/
│
└── docs/
    ├── architecture/
    ├── api/
    └── design/
```

The structure is intentionally subject to refinement as the domain model becomes more concrete.

---

# Design Priorities

Development should proceed in the following order:

1. **Define the domain model**

   * Session
   * Event
   * Scene
   * Camera
   * Filter
   * Geometry
   * Visualisation

2. **Establish PandoraSDK compatibility**

   * preserve existing API entry points;
   * implement compatibility over the new model.

3. **Remove hardcoded scene/view assumptions**

   * replace fixed viewers and scene members with managed collections.

4. **Separate persistent and event-level state**

5. **Generalise slicing into filtering**

6. **Separate the visualisation model from ROOT/TEve**

7. **Implement the interactive GUI**

8. **Implement session persistence**

9. **Expand offline and headless capabilities**

This order is intentional: the GUI should be built on a stable visualisation model rather than becoming the source of that model.

---

# Non-Goals

FCCVisualization is not intended to:

* replace PandoraSDK;
* replace Pandora reconstruction algorithms;
* replace ROOT;
* replace TEve;
* require changes to Pandora algorithms merely to support the new implementation;
* become a general-purpose 3D engine;
* encode detector-specific assumptions into the core framework;
* make GUI functionality mandatory for batch processing.

---

# Dependencies

The primary intended dependencies are:

* **PandoraSDK**
* **ROOT**
* **TEve**
* C++ and the standard library
* CMake

Additional dependencies should be introduced only when they provide a clear architectural benefit.

---

# Development Philosophy

FCCVisualization should favour **composition over specialisation**.

New functionality should preferably be introduced by adding:

```text
new scene configuration
new filter
new visualisation
new renderer capability
new data source
```

rather than by adding another special-case branch to the central monitoring implementation.

In particular:

> **The existence of a new detector view should never require adding another hardcoded viewer member to the monitoring class.**

Likewise:

> **The existence of a new filtering criterion should never require modifying the scene implementation itself.**

The framework should provide the abstractions necessary for these extensions to be expressed as data and behaviour rather than hardcoded infrastructure.

---

# Long-Term Direction

The long-term goal is a monitoring environment in which an event can be explored interactively through a collection of user-defined visualisations.

For example:

```text
Session
│
├── Scene: "Full Detector"
│   ├── 3D Perspective
│   ├── Detector Geometry
│   └── All Objects
│
├── Scene: "Central Region"
│   ├── XY Orthographic
│   ├── Spatial Box Filter
│   └── Tracks + CaloHits
│
├── Scene: "High Energy"
│   ├── 3D Perspective
│   ├── Energy Filter
│   └── PFOs + MC Particles
│
└── Scene: "Muon"
    ├── XZ Orthographic
    ├── PDG Filter
    └── Tracks + MC Particles
```

The user should be able to move between events while retaining these configurations.

Ultimately, a saved session should allow the complete visualisation environment to be reconstructed without rebuilding it manually.

---

# Contributing

FCCVisualization is developed as part of the FCC software ecosystem.

Contributions should preserve the architectural principles described in this document, particularly:

* PandoraSDK compatibility;
* separation of domain model and rendering backend;
* persistent scene management;
* composable filtering;
* frontend independence;
* avoidance of hardcoded detector/view assumptions.

Before introducing a new abstraction, prefer determining whether the functionality belongs to an existing conceptual layer.

---

# License

FCCVisualization is distributed under the **Apache License, Version 2.0**.

Copyright © 2026 Independent Society of Knowledge (ISK) / FCCVisualization contributors.

See [`LICENSE`](LICENSE) for the complete license text.

---

# Maintainer

**Primary developer:** Amir Ebrahimnezhad

**Email:** [amir.ebh@cern.ch](mailto:amir.ebh@cern.ch)

---

# Project Identity

**Project:** FCCVisualization

**Purpose:** Generalised event visualisation and interactive monitoring for FCC/PandoraSDK

**Primary rendering technology:** ROOT / TEve

**Primary integration:** PandoraSDK

**License:** Apache License 2.0
