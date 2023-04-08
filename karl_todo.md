## Next up

- Consolidate Mesh::Vertices/Normals/Indices triple into its own MeshBuffers struct
- Add toggle to create/view tetrahedral version of mesh
  - Save .obj to tmp/mesh.obj
  - Load .obj into vega/ObjMesh instance
  - Delete tmp
  - Transform vega/ObjMesh into vega/TetMesh
    - Provide config over conversion params
  - Transform vega/TetMesh vertices/normals/indices to a new mesh2audio/MeshBuffer, and cache in mesh2audio/Mesh (similar to how MeshProfile is stored in it).
    - Cache the TetMesh while we're at it
  - Add Triangular/Tetrahedral mesh toggle
- Pass the cached TetMesh to the mesh2faust method to transform it to Faust code
- Display the Faust code in the text editor and update the dsp
- Make the text editor non-editable (read-only)
- Use Ben's axisymmetric solver (via file IO)
  - Add a toggle between the tet mesh vs axisymmetric audio model
- Add material controls for audio model
- Implement UI for the generated Faust code
- Mouse wheel camera zoom
