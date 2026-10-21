# FCCVisualization

**A modular, scene-based visualization toolkit for FCC detector geometry and event data.**

FCCVisualization provides a persistent command-line workflow for loading high-energy physics data, inspecting detector geometry, composing visualization scenes, and rendering them through interchangeable backends.

The project is designed to keep data adapters, scene configuration, filtering, styling, and rendering separate, making it easier to extend and reuse visualization workflows.

## Features

- **Event data:** Load EDM4hep ROOT files and inspect collections and their attributes.
- **Detector geometry:** Import DD4hep compact detector descriptions and inspect geometry hierarchies.
- **Persistent scenes:** Compose reusable scenes from multiple data and geometry layers.
- **Filtering and styling:** Select data by attributes or spatial constraints and customize its appearance.
- **Multiple backends:** Support interactive visualization and programmatic output through backend adapters.
- **Scriptable workflow:** Configure and reproduce visualizations through CLI commands.

## Quick start

Clone the repository:

```bash
git clone https://github.com/thisismeamir/FCCVisualization.git
cd FCCVisualization
```

Build instructions and dependencies are documented in the [project documentation](https://fccvisualization.readthedocs.io/).

Once built, use the `fccvis` CLI to load data and configure a scene. For example:

```text
fccvis> load edm4hep /path/to/events.edm4hep.root
fccvis> load detector /path/to/detector.xml
fccvis> scene new event
fccvis> layer add event MCParticles
fccvis> layer add event geometry/Tracker
```

Consult the documentation for the exact command syntax, available adapters, filtering expressions, styling options, and backend configuration.

## Architecture

FCCVisualization is organized around three main concerns:

- **Adapters:** Convert external data sources into the common data model.
- **Scene and data model:** Manage series, layers, filters, styles, and visualization state.
- **Rendering backends:** Translate scene state into backend-specific output.

This separation allows new input formats and rendering targets to be introduced without coupling them to the entire application.

## Documentation

- [User guide and CLI reference](https://fccvisualization.readthedocs.io/)
- [Source code and issue tracker](https://github.com/thisismeamir/FCCVisualization)

## Development status

FCCVisualization is under active development. Features and interfaces may evolve as the project approaches its v1.0 release.

Bug reports, feature requests, and contributions are welcome through GitHub issues and pull requests.

## License

See the repository's `LICENSE` file for licensing terms.
