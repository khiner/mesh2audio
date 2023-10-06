#pragma once

#include <memory>
#include <vector>

#include "Geometry/Geometry.h"

struct btRigidBody;
struct btCollisionShape;
struct btDefaultMotionState;
struct btCollisionShape;
struct btTriangleMesh;

struct RigidBody {
    RigidBody(Geometry *);
    RigidBody(glm::vec3 plane_normal, const glm::vec3 &initial_pos = {0, 0, 0});
    ~RigidBody();

    void Tick();

    btRigidBody *GetBody() const;

private:
    Geometry *geometry_;
    std::unique_ptr<btTriangleMesh> mesh_;
    std::unique_ptr<btCollisionShape> shape_;
    std::unique_ptr<btDefaultMotionState> motionState_;
    std::unique_ptr<btRigidBody> body_;
};

struct Physics {
    Physics();
    ~Physics();

    void AddRigidBody(Geometry *);
    void AddRigidBody(glm::vec3 plane_normal, const glm::vec3 &initial_pos = {0, 0, 0});

    void Tick();

    void RenderConfig();

    float Gravity = -9.81;

private:
    std::vector<std::unique_ptr<RigidBody>> RigidBodies;
};
