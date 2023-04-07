## Next up

- Consolidate Mesh::Vertices/Normals/Indices triple into its own MeshBuffers struct
- Export faust2mesh as a lib
  - Provide function that takes a vega/ObjMesh, and one that takes a vega/TetMesh
- Add toggle to create/view tetrahedral version of mesh
  - Save .obj to tmp/mesh.obj
  - Load .obj into vega/ObjMesh instance
  - Transform vega/Objmesh into vega/TetMesh
    - Provide config over conversion params
  - Transform vega/TetMesh vertices/normals/indices to a new mesh2audio/MeshBuffer, and cache in mesh2audio/Mesh (similar to how MeshProfile is stored in it).
    - Cache the TetMesh while we're at it
  - Add Tetrahedral/Radial (Triangle?) mesh toggle
- Pass the cached TetMesh to the faust2mesh method to transform it to Faust code
- Display the Faust code in the text editor and update the dsp
- Make the text editor non-editable (read-only)
- Use Ben's axisymmetric solver (via file IO)
  - Add a toggle between the tet mesh vs axisymmetric audio model
- Add material controls for audio model
