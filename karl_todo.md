## Next up

- Link the generated dsp with the tet mesh used to generate it (the tet mesh active at dsp gen time)
- Disable 'Generate DSP' button when tet mesh / dsp params are still the same as they were for the most recent dsp gen
  - Add `DspGenDirty` flag to `Audio`
- Add params to control all mesh2faust args
- Excitation positions:
  - Limit range of excitation position NumEntry
  - Show excitation position points & labels on mesh, and do a little brightness animation when struck
  - Hover & click directly on enabled excitation pos to strike
- Add controls for tet mesh generation params (how fine grained etc)
- Explore the effect of the mesh scale (uniform) on the bell model
  - Add scale control for dsp generation
- Waveform & spectrogram
- Calculate mode frequencies empirically for validation against known bell modes
- Use axisymmetric solver (via file IO)
  - Add a toggle between the tet mesh vs axisymmetric audio model
- Mouse wheel camera zoom
