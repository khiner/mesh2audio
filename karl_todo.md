## Next up

- Show loading modal/spinner when creating tet mesh and generating dsp
- See if we can load Vega::tetMesh directly as well
  - For obj files that are already tetrahedral, or for trying a different, faster tet mesher, e.g. [fTetWild](https://github.com/wildmeshing/fTetWild)
  - Note: fTetWild depends on [geogram](https://github.com/BrunoLevy/geogram),
    which has its own tet mesher via
    [tetgen](https://github.com/ufz/tetgen),
    so may want to just start with that.
    - Use `GEOGRAM_LIB_ONLY` to exclude programs & viewer
    - Use `GEOGRAM_WITH_TETGEN` to include tetgen
- Explore the effect of the mesh scale (uniform) on the bell model
  - Add scale control for dsp generation
- Add Triangular/Tetrahedral toggle to mesh viewer if tet mesh is present
- Add controls for tet mesh generation params (how fine grained etc)
- Use Ben's axisymmetric solver (via file IO)
  - Add a toggle between the tet mesh vs axisymmetric audio model
- Add material controls for audio model
- Implement UI for the generated Faust code
- Make the text editor non-editable (read-only)
- Mouse wheel camera zoom
