#pragma once

#include "Mesh/Mesh.h"

struct Wireframe {
    Wireframe(Mesh *);

    Mesh *ParentMesh;
    std::vector<Mesh> Meshes;
};
