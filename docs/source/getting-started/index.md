# Getting Started


## About this Project

FCCVisualization has four primary goals.

1. Generalize PandoraMonitoring: The existing PandoraMonitoring implementation lacks a modularized, general design for visualizations. In my mind it should support:
    - Dynamically defined scenes;
    - Configurable Cameras and Projections;
    - Configurable Detector Geometry and Detector Geometry Content;
    - Reusable Visualization Configurations outside the main code (via options.cc files);
    - Configurable rendering properties;
    - Spatial Slicing, Filters based on any property, detector slicing etc.
    - Event Navigation;
    - Interactive Inspection of Visualization, live adjustments through interactive shell.

The system should make it possible to add new visualization concepts without the need to modify the source code itself.

2. Maintain PandoraSDK: FCCVisualization is intended to work with **PandoraSDK** and must therefore treat it correctly.

3. Interactivity and live work on visualizations instead of single-shot runs. Through shell (ROOT gInterpreter), gui (TApplication), or a combination of them.

4. Contain a set of preconfigurations for FCC specific needs.


## Application Cycle

Every Visualization starts with a defined set of data (`.edm4hep.root` file) given to a session. The session is the first object that is going to be created once `fccvis` command is running. 

```{mermaid}
mindmap
  root((FCCVisualization))
    Visualization
      Scenes
      Cameras
      Filters
      Rendering
    Data
      EDM4hep
      podio
      Pandora
    Analysis
      FCCAnalyses
      Histograms
    Interaction
      Interactive Shell
      Cling
      Session
``` 
