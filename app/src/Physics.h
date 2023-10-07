#pragma once

#include "Geometry/Geometry.h"

namespace reactphysics3d {
class RigidBody;
}

struct RigidBody {
    reactphysics3d::RigidBody *Body;
    Geometry *Geometry;
};

struct Physics {
    Physics();
    ~Physics();

    void AddRigidBody(Geometry *);
    void AddRigidBody(const glm::vec3 &initial_pos = {0, 0, 0});

    void Tick();

    void RenderConfig();

    float Gravity = -9.81;

private:
    std::vector<RigidBody> RigidBodies;
};
