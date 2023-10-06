#pragma once

#include <memory>
#include <vector>

#include "Geometry/Geometry.h"

#include <reactphysics3d/reactphysics3d.h>

struct RigidBody {
    rp3d::RigidBody *Body;
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
