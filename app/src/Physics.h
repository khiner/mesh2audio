#pragma once

#include "Mesh/Mesh.h"

namespace reactphysics3d {
class RigidBody;
}

struct RigidBody {
    reactphysics3d::RigidBody *Body;
    Mesh *Mesh;
};

struct Physics {
    Physics();
    ~Physics();

    void AddRigidBody(Mesh *);
    void AddRigidBody(const glm::vec3 &initial_pos = {0, 0, 0});

    void Tick();

    void RenderConfig();

    float Gravity = -9.81;

private:
    std::vector<RigidBody> RigidBodies;
};
