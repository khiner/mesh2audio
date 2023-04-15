## Next up

- Add material controls for audio model
- Add controls for tet mesh generation params (how fine grained etc)
- Explore the effect of the mesh scale (uniform) on the bell model
  - Add scale control for dsp generation
- Implement UI for the generated Faust code
  - Make the text editor non-editable (read-only) by default
  - Editing removes faust UI (which is specialized to assuming the default model dsp code)
- Waveform & spectrogram
- Calculate mode frequencies empirically for validation against known bell modes
- Use axisymmetric solver (via file IO)
  - Add a toggle between the tet mesh vs axisymmetric audio model
- Mouse wheel camera zoom
