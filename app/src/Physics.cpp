#include "Physics.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"

#include <btBulletDynamicsCommon.h>

#include <iostream>

static btVector3 Glm2Bt(const glm::vec3 &vec) { return {vec.x, vec.y, vec.z}; }

static glm::mat4 Bt2Glm(const btTransform &in) {
    glm::mat4 out;
    // Convert rotation quaternion to a 3x3 matrix.
    btMatrix3x3 btBasis = in.getBasis();
    for (size_t r = 0; r < 3; r++) {
        for (size_t c = 0; c < 3; c++) {
            out[c][r] = btBasis[r][c]; // Transposed
        }
    }

    btVector3 origin = in.getOrigin();
    out[3][0] = origin.x();
    out[3][1] = origin.y();
    out[3][2] = origin.z();

    out[0][3] = 0.0f;
    out[1][3] = 0.0f;
    out[2][3] = 0.0f;
    out[3][3] = 1.0f;

    return out;
}
static btTransform Glm2Bt(const glm::mat4 &in) {
    btMatrix3x3 basis(
        in[0][0], in[1][0], in[2][0],
        in[0][1], in[1][1], in[2][1],
        in[0][2], in[1][2], in[2][2]
    );

    btVector3 origin(in[3][0], in[3][1], in[3][2]);

    return btTransform(basis, origin);
}

RigidBody::RigidBody(Geometry *geom)
    : geometry_(geom), mesh_(std::make_unique<btTriangleMesh>()) {
    glm::mat4 transform = geom->Transforms[0];
    // const auto &indices = geom->TriangleIndices;
    // for (size_t i = 0; i < indices.size(); i += 3) {
    //     mesh_->addTriangle(
    //         Glm2Bt(geom->Vertices[indices[i]]),
    //         Glm2Bt(geom->Vertices[indices[i + 1]]),
    //         Glm2Bt(geom->Vertices[indices[i + 2]])
    //     );
    // }
    // shape_ = std::make_unique<btBvhTriangleMeshShape>(mesh_.get(), true);
    // motionState_ = std::make_unique<btDefaultMotionState>(Glm2Bt(transform));

    const auto [geom_min, geom_max] = geom->ComputeBounds();
    glm::vec3 half_extents = (geom_max - geom_min) * 0.5f;
    shape_ = std::make_unique<btBoxShape>(Glm2Bt(half_extents));
    motionState_ = std::make_unique<btDefaultMotionState>(Glm2Bt(transform));

    btScalar mass = 0.1f;
    btVector3 inertia(0, 0, 0);
    shape_->calculateLocalInertia(mass, inertia);
    btRigidBody::btRigidBodyConstructionInfo ci(mass, motionState_.get(), shape_.get(), inertia);
    body_ = std::make_unique<btRigidBody>(ci);
}

RigidBody::RigidBody(glm::vec3 plane_normal, const glm::vec3 &initial_pos)
    : mesh_(std::make_unique<btTriangleMesh>()) {
    shape_ = std::make_unique<btStaticPlaneShape>(Glm2Bt(plane_normal), 0);
    motionState_ = std::make_unique<btDefaultMotionState>(btTransform({0, 0, 0, 1}, Glm2Bt(initial_pos)));

    btRigidBody::btRigidBodyConstructionInfo ci(0, motionState_.get(), shape_.get()); // Mass set to 0 for static plane
    body_ = std::make_unique<btRigidBody>(ci);
}

RigidBody::~RigidBody() {
    // Automatic cleanup in reverse order of creation when the RigidBody object is destroyed
}

void RigidBody::Tick() {
    btTransform transform;
    GetBody()->getMotionState()->getWorldTransform(transform);
    if (geometry_) {
        std::cout << "Transform: " << transform.getOrigin().getX() << ", " << transform.getOrigin().getY() << ", " << transform.getOrigin().getZ() << std::endl;
        geometry_->SetTransform(Bt2Glm(transform));
    } else {
        std::cout << "PT: " << transform.getOrigin().getX() << ", " << transform.getOrigin().getY() << ", " << transform.getOrigin().getZ() << std::endl;
    }
}

btRigidBody *RigidBody::GetBody() const { return body_.get(); }

btDefaultCollisionConfiguration CollisionConfiguration;
btCollisionDispatcher CollisionDispatcher(&CollisionConfiguration);
btDbvtBroadphase Broadphase;
btSequentialImpulseConstraintSolver Solver;
btDiscreteDynamicsWorld DynamicsWorld(&CollisionDispatcher, &Broadphase, &Solver, &CollisionConfiguration);

Physics::Physics() {
    DynamicsWorld.setGravity({0, Gravity, 0});
}

Physics::~Physics() {
    for (auto &rigid_body : RigidBodies) {
        DynamicsWorld.removeRigidBody(rigid_body->GetBody());
    }
}

void Physics::AddRigidBody(Geometry *geometry) {
    auto rigid_body = std::make_unique<RigidBody>(geometry);
    DynamicsWorld.addRigidBody(rigid_body->GetBody());
    RigidBodies.push_back(std::move(rigid_body));
}
void Physics::AddRigidBody(glm::vec3 plane_normal, const glm::vec3 &initial_pos) {
    auto rigid_body = std::make_unique<RigidBody>(std::move(plane_normal), initial_pos);
    DynamicsWorld.addRigidBody(rigid_body->GetBody());
    RigidBodies.push_back(std::move(rigid_body));
}

void Physics::Tick() {
    DynamicsWorld.stepSimulation(1.0f / 60.0f, 10);

    for (auto &rigid_body : RigidBodies) {
        rigid_body->Tick();
    }
}

using namespace ImGui;

void Physics::RenderConfig() {
    SliderFloat("Gravity", &Gravity, -20, 20);
}
