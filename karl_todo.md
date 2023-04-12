## Next up

- Show loading modal/spinner when creating tet mesh and generating dsp
- Load Vega::objMesh by setting vert/faces manually instead of saving to temp obj file
- See if we can load Vega::tetMesh directly as well
  - For obj files that are already tetrahedral, or for trying a different, faster tet mesher, e.g. [TetWild](https://github.com/Yixin-Hu/TetWild)
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
