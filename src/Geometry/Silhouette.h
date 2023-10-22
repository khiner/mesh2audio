#pragma once

#include <OpenMesh/Core/Mesh/PolyMesh_ArrayKernelT.hh>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

// Define the mesh type
using MeshType = OpenMesh::PolyMesh_ArrayKernelT<>;

// TODO pass face normals and centroids to the shader and find silhouette edges there.
std::vector<MeshType::EdgeHandle> FindSilhouetteEdges(const MeshType &mesh, const glm::mat4 &mesh_transform, const glm::vec3 &camera_pos);
