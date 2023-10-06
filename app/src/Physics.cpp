#include "Physics.h"

#include <iostream>

rp3d::PhysicsCommon PhysicsCommon;
rp3d::PhysicsWorld *World;

static rp3d::Vector3 Glm2Rp3d(const glm::vec3 &vec) { return {vec.x, vec.y, vec.z}; }

static glm::mat4 Rp3d2Glm(const rp3d::Transform &transform) {
    glm::mat4 result;
    transform.getOpenGLMatrix(&result[0][0]);
    return result;
}

static rp3d::Transform Glm2Rp3d(glm::mat4 &transform) {
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

void Physics::AddRigidBody(Geometry *geometry) {
    // todo this is not working well. meshes can tunnel through ground in certain (symmetric) positions, and errors for many meshes.
    // rp3d::VertexArray vertex_array(&geometry->Vertices[0], sizeof(glm::vec3), geometry->Vertices.size(), rp3d::VertexArray::DataType::VERTEX_FLOAT_TYPE);
    // std::vector<rp3d::Message> messages;
    // rp3d::ConvexMesh *convex_mesh = PhysicsCommon.createConvexMesh(vertex_array, messages);
    // if (convex_mesh == nullptr) {
    //     std::cout << "Error while creating a ConvexMesh:" << std::endl;
    //     for (const auto &message : messages) std::cout << "Error: " << message.text << std::endl;
    // }
    // auto *shape = PhysicsCommon.createConvexMeshShape(convex_mesh);

    auto [geom_min, geom_max] = geometry->ComputeBounds();
    auto *shape = PhysicsCommon.createBoxShape(Glm2Rp3d((geom_max - geom_min) * 0.5f));

    auto *body = World->createRigidBody(Glm2Rp3d(geometry->Transforms[0]));
    body->setMass(1.f);
    body->addCollider(shape, {});
    body->setIsAllowedToSleep(false);
    RigidBodies.push_back({body, geometry});
}

void Physics::AddRigidBody(const glm::vec3 &initial_pos) {
    auto transform = rp3d::Transform::identity();
    transform.setPosition(Glm2Rp3d(initial_pos - glm::vec3{0, 1, 0})); // Make room for box representing plane.
    auto *body = World->createRigidBody(transform);
    body->setType(rp3d::BodyType::STATIC);
    rp3d::Vector3 floor_half_extents(100, 1, 100);
    auto *shape = PhysicsCommon.createBoxShape(floor_half_extents);
    body->addCollider(shape, {});
    RigidBodies.push_back({body, nullptr});
}

void Physics::Tick() {
    World->update(1.0f / 60.0f);
    for (auto &rigid_body : RigidBodies) {
        if (rigid_body.Geometry) {
            rigid_body.Geometry->SetTransform(Rp3d2Glm(rigid_body.Body->getTransform()));
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
