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

    void AddRigidBody(Mesh *, BodyType = BodyType::Dynamic);

    void Tick();

    void RenderConfig();

    float Gravity = -9.81;

private:
    std::vector<RigidBody> RigidBodies;
};
