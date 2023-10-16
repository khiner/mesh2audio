#pragma once

#include "Mesh/Mesh.h"

namespace reactphysics3d {
class RigidBody;
} // namespace reactphysics3d

struct RigidBody {
    reactphysics3d::RigidBody *Body;
    Mesh *Mesh;
};

struct Physics {
    // Mirrors `rp3d::BodyType`.
    enum class BodyType {
        Static,
        Kinematic,
        Dynamic,
    };

    Physics();
    ~Physics();

    /**
        If `is_concave = false`, the mesh is assumed to be convex.
        Otherwise, the mesh is assumed to be concave, and a convex hull is generated from it.
        Convex meshes must meet the following criteria from https://www.reactphysics3d.com/usermanual.html:
            - The mesh is indeed convex.
            - Simplest possible convex mesh.
                - Avoid coplanar faces.
                - Coplanar faces must be merged together.
            - No duplicate vertices.
                - The number of vertices passed to create `PolygonVertexArray` must be exactly the number of vertices in the convex mesh.
            - Vertices of a face must be specified in CCW order, as seen from the outside of the convex mesh.
            - Origin must be inside the convex mesh.
    */
    void AddRigidBody(Mesh *, BodyType = BodyType::Dynamic, bool is_concave = false);

    void Tick();

    void RenderConfig();

    float Gravity = -9.81;

private:
    std::vector<RigidBody> RigidBodies;
};
