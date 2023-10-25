#include "Physics.h"

#include <reactphysics3d/reactphysics3d.h>

#include "Geometry/ConvexHull.h"

rp3d::PhysicsCommon PhysicsCommon;
rp3d::PhysicsWorld *World;

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

static glm::vec3 Rp3d2Glm(const rp3d::Vector3 &v) { return {v.x, v.y, v.z}; }

// Accumulates all collisions in the most recent frame into a `Collisions` vector.
struct CollisionAccumulator : reactphysics3d::CollisionCallback {
    using Collision = Physics::Collision;

    CollisionAccumulator(std::vector<RigidBody> *rigid_bodies) : RigidBodies(rigid_bodies) {}

    void onContact(const CallbackData &data) override {
        Collisions.clear();

        for (uint i = 0; i < data.getNbContactPairs(); ++i) {
            const auto pair = data.getContactPair(i);
            for (uint j = 0; j < pair.getNbContactPoints(); ++j) {
                const auto point = pair.getContactPoint(j);
                auto *body1 = FindRigidBody(pair.getBody1());
                auto *body2 = FindRigidBody(pair.getBody2());
                if (body1 == nullptr || body2 == nullptr) {
                    std::cerr << "Error: could not find rigid body for collision." << std::endl;
                    continue;
                }
                Collisions.emplace_back(
                    Collision::Point{body1, Rp3d2Glm(point.getLocalPointOnCollider1())},
                    Collision::Point{body2, Rp3d2Glm(point.getLocalPointOnCollider2())},
                    Rp3d2Glm(point.getWorldNormal()),
                    point.getPenetrationDepth()
                );
            }
        }
    }

    std::vector<Collision> Collisions;
    std::vector<RigidBody> *RigidBodies;

private:
    RigidBody *FindRigidBody(rp3d::Body *body) {
        for (auto &rigid_body : *RigidBodies) {
            if (rigid_body.Body == body) return &rigid_body;
        }
        return nullptr;
    }
};

static rp3d::BodyType ConvertBodyType(Physics::BodyType body_type) {
    switch (body_type) {
        case Physics::BodyType::Static: return rp3d::BodyType::STATIC;
        case Physics::BodyType::Kinematic: return rp3d::BodyType::KINEMATIC;
        case Physics::BodyType::Dynamic: return rp3d::BodyType::DYNAMIC;
    }
}

Physics::Physics() {
    rp3d::PhysicsWorld::WorldSettings settings;
    // settings.defaultVelocitySolverNbIterations = 20;
    settings.isSleepingEnabled = false;
    settings.gravity = {0, Gravity, 0};
    World = PhysicsCommon.createPhysicsWorld(std::move(settings));
    CollisionCallback = std::make_unique<CollisionAccumulator>(&RigidBodies);
}

Physics::~Physics() {
    PhysicsCommon.destroyPhysicsWorld(World);
}

rp3d::ConvexMesh *OpenMeshToConvexMesh(const Geometry::MeshType &mesh) {
    // Copy the OpenMesh indices. (Points can be directly copied.)
    std::vector<uint> indices;
    std::vector<rp3d::PolygonVertexArray::PolygonFace> poly_faces;
    poly_faces.reserve(mesh.n_faces());
    for (const auto &face : mesh.faces()) {
        const uint num_vertices = face.valence();
        const uint index_base = indices.size(); // Index of the first vertex of the polygon face inside the array of vertex indices.
        poly_faces.emplace_back(num_vertices, index_base);
        auto v_it = mesh.cfv_iter(face);
        for (size_t i = 0; i < num_vertices; i++) {
            indices.push_back(v_it->idx());
            ++v_it;
        }
    }

    const rp3d::PolygonVertexArray poly_vertices = {
        uint(mesh.n_vertices()), mesh.points(), 3 * sizeof(float), &indices[0], sizeof(uint), uint(mesh.n_faces()), &poly_faces[0],
        rp3d::PolygonVertexArray::VertexDataType::VERTEX_FLOAT_TYPE, rp3d::PolygonVertexArray::IndexDataType::INDEX_INTEGER_TYPE};

    std::vector<rp3d::Message> messages;
    auto *convex_mesh = ::PhysicsCommon.createConvexMesh(poly_vertices, messages);
    if (convex_mesh == nullptr) {
        std::cerr << "Error while creating a ConvexMesh:" << std::endl;
        for (const auto &message : messages) std::cerr << "Error: " << message.text << std::endl;
    }

    return convex_mesh;
}

void Physics::AddRigidBody(Mesh *mesh, BodyType body_type, bool is_concave) {
    // todo this is not working well. meshes can tunnel through ground in certain (symmetric) positions, and errors for many meshes.
    rp3d::ConvexMesh *convex_mesh = is_concave ? ConvexHull::GenerateConvexMesh(mesh->GetPolyhedron().GetVertices(), mesh->GetPolyhedron().NumVertices()) : OpenMeshToConvexMesh(mesh->GetPolyhedron().GetMesh());
    auto *shape = PhysicsCommon.createConvexMeshShape(convex_mesh);

    // Uncomment to use bounding box for collisions.
    // auto *shape = PhysicsCommon.createBoxShape(Glm2Rp3d((geom_max - geom_min) * 0.5f));

    auto *body = World->createRigidBody(Glm2Rp3d(mesh->GetTransform()));
    body->setType(ConvertBodyType(body_type));
    body->setMass(1.f);
    body->addCollider(shape, {});
    RigidBodies.push_back({body, mesh});
}

const std::vector<Physics::Collision> &Physics::Tick() {
    static const float dt = 1.f / 60.f;
    World->update(dt);
    for (auto &rigid_body : RigidBodies) {
        if (rigid_body.Mesh) {
            rigid_body.Mesh->SetTransform(Rp3d2Glm(rigid_body.Body->getTransform()));
        }
    }
    World->testCollision(*CollisionCallback);

    return CollisionCallback->Collisions;
}

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"

using namespace ImGui;

void Physics::RenderConfig() {
    if (SliderFloat("Gravity", &Gravity, -20, 20)) {
        World->setGravity({0, Gravity, 0});
    }
}
