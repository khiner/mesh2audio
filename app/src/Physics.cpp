#include "Physics.h"

#include <reactphysics3d/reactphysics3d.h>

#include "Geometry/ConvexHull.h"

rp3d::PhysicsCommon PhysicsCommon;
rp3d::PhysicsWorld *World;

static rp3d::BodyType ConvertBodyType(Physics::BodyType body_type) {
    switch (body_type) {
        case Physics::BodyType::Static: return rp3d::BodyType::STATIC;
        case Physics::BodyType::Kinematic: return rp3d::BodyType::KINEMATIC;
        case Physics::BodyType::Dynamic: return rp3d::BodyType::DYNAMIC;
    }
}

static glm::mat4 Rp3d2Glm(const rp3d::Transform &transform) {
    glm::mat4 result;
    transform.getOpenGLMatrix(&result[0][0]);
    return result;
}

static rp3d::Transform Glm2Rp3d(glm::mat4 transform) {
    rp3d::Transform result;
    result.setFromOpenGL(&transform[0][0]);
    return result;
}

Physics::Physics() {
    rp3d::PhysicsWorld::WorldSettings settings;
    // settings.defaultVelocitySolverNbIterations = 20;
    // settings.isSleepingEnabled = false;
    settings.gravity = {0, Gravity, 0};
    World = PhysicsCommon.createPhysicsWorld(std::move(settings));
}

Physics::~Physics() {
    PhysicsCommon.destroyPhysicsWorld(World);
}

void Physics::AddRigidBody(Mesh *mesh, BodyType body_type) {
    // todo this is not working well. meshes can tunnel through ground in certain (symmetric) positions, and errors for many meshes.
    rp3d::ConvexMesh *convex_mesh = ConvexHull::GenerateConvexMesh(mesh->GetTriangles().GetVertices());
    auto *shape = PhysicsCommon.createConvexMeshShape(convex_mesh);

    // Uncomment to use bounding box for collisions.
    // auto *shape = PhysicsCommon.createBoxShape(Glm2Rp3d((geom_max - geom_min) * 0.5f));

    auto *body = World->createRigidBody(Glm2Rp3d(mesh->GetTransform()));
    body->setType(ConvertBodyType(body_type));
    body->setMass(1.f);
    body->addCollider(shape, {});
    body->setIsAllowedToSleep(false);
    RigidBodies.push_back({body, mesh});
}

void Physics::Tick() {
    World->update(1.0f / 60.0f);
    for (auto &rigid_body : RigidBodies) {
        if (rigid_body.Mesh) {
            rigid_body.Mesh->SetTransform(Rp3d2Glm(rigid_body.Body->getTransform()));
        }
    }
}

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"

using namespace ImGui;

void Physics::RenderConfig() {
    if (SliderFloat("Gravity", &Gravity, -20, 20)) {
        World->setGravity({0, Gravity, 0});
    }
}
