# mesh2audio

Convert a volumetric mesh to a physical audio model. Based on [mesh2faust](https://hal.science/hal-03162901/document).

## `mesh2faust` architecture

![](mesh2faust_impl_overview.png)

## Why?

Convert 3D meshes to a playable instrument with excitable vertices - what's not to love?

3D meshes are everywhere.
We use them for visual rendering, but they contain information we can extract audio from (esp. when paired with material properties).

Small steps to use-cases like physically-plausible video2audio.
(Estimate time-varying meshes + materials from video. Estimate collisions between meshes. Converty vertices + materials + collisions to audio stream.)

An implementation already exists (mesh2faust).
So what's missing?

- Very slow to compile.
  - ["To give you an idea, analyzing a mesh with ~3E4 faces will probably take about 15 minutes on a regular laptop"](https://ccrma.stanford.edu/~rmichon/faustTutorials/_
  - todo measure perf on my machine (KH)
- No visual interface at all.
  - Can't control mesh in real-time.
    (It relies on the user exporting a mesh from something like OpenScad->MeshLab, in a compatible format.)
  - Potential feature: Interface to change [some aspects of] 3D mesh in real-time
- Limited configuration
  - E.g. constants representing material properties are baked in.
    Changing constants requires a full recompilation.
- Implementation relies on compiling to Faust, which introduces a dependency and a significant part of the compile times.
- Aspects of implementation obscured by 3rd party libraries (Vega FEM)
- Uses MKL, which is closed source
  - Could use a newer open-source BLAS implementation like OpenBLAS or BLIS, or another vectorization lib, depending on env/lang.
- ...
