# Kosmic Docs

Kosmic is a general-purpose, multi-facade lattice field theory simulator, developed with cosmological research in mind. 
The main goal of kosmic is to be able to provide:

1. A generic field-theory simulator, in a generic metric (currently only FLRW and Minkowski).
2. Modular and Extensibility for further ad-hoc developments, extractions and research-specific pipelines.
3. Optimizations, and modern approaches to computation of lattice field theory, scpecially for cosmology.

Kosmic provides clean and modular interface to define new theories, metrics, and aims to support different backends (cpu, gpu).
It also provides algorithmic methods such as adaptive-mesh refinement and augmentation, out-of-the-box, to gain better performance for large ensemble requirements such as simulation-based inference pipelines.


In this documentation we provide necessary knowledge to start working with kosmic, and to develop custom simulations. 

```{toctree}
:maxdepth: 2
:numbered:

getting-started/index


```
