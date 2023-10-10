#include "Physics.h"

#include <reactphysics3d/reactphysics3d.h>

#include "Geometry/ConvexHull.h"

rp3d::PhysicsCommon PhysicsCommon;
rp3d::PhysicsWorld *World;

static rp3d::Vector3 Glm2Rp3d(const glm::vec3 &vec) { return {vec.x, vec.y, vec.z}; }

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

// static std::unique_ptr<rp3d::PolygonVertexArray> PolygonVertices;
// static std::vector<rp3d::PolygonVertexArray::PolygonFace> PolygonFaces;
// rp3d::PolyhedronMesh *GeometryDataToPolyhedronMesh(const GeometryData &data) {
//     using namespace rp3d;
//     const auto &vertices = data.Vertices;
//     const auto &indices = data.Indices;

//     PolygonFaces.resize(data.Indices.size() / 3);
//     for (uint f = 0; f < PolygonFaces.size(); f++) {
//         PolygonFaces[f].indexBase = f * 3; // First vertex of the face in the indices array.
//         PolygonFaces[f].nbVertices = 3; // Number of vertices in the face.
//     }

//     PolygonVertices = std::make_unique<PolygonVertexArray>(
//         vertices.size(), &vertices[0][0], 3 * sizeof(float), &indices[0], sizeof(int), indices.size(), &PolygonFaces[0],
//         PolygonVertexArray::VertexDataType::VERTEX_FLOAT_TYPE, PolygonVertexArray::IndexDataType::INDEX_INTEGER_TYPE
//     );
//     /** todo failed assert in `PhysicsCommon.createPolyhedronMesh`.
//        Look for a way to find a minimal polygon meeting the following criteria from https://www.reactphysics3d.com/usermanual.html:
//        - You need to make sure that the mesh you provide is indeed convex.
//          Secondly, you should provide the simplest possible convex mesh.
//          This means that you need to avoid coplanar faces in your convex mesh shape.
//          Coplanar faces have to be merged together.
//          Remember that convex meshes are not limited to triangular faces, you can create faces with more than three vertices.
//        - Also note that meshes with duplicated vertices are not supported.
//          The number of vertices you pass to create the PolygonVertexArray must be exactly the number of vertices in your convex mesh.
//        - When you specify the vertices for each face of your convex mesh, be careful with their order.
//          The vertices of a face must be specified in counter clockwise order as seen from the outside of your convex mesh.
//        - You also need to make sure that the origin of your mesh is inside the convex mesh.
//          A mesh with an origin outside the convex mesh is not currently supported by the library.
//     */
//     return ::PhysicsCommon.createPolyhedronMesh(PolygonVertices.get());
// }

void Physics::AddRigidBody(Mesh *mesh) {
    // todo this is not working well. meshes can tunnel through ground in certain (symmetric) positions, and errors for many meshes.
    rp3d::ConvexMesh *convex_mesh = ConvexHull::GenerateConvexMesh(mesh->GetTriangles().GetVertices());
    auto *shape = PhysicsCommon.createConvexMeshShape(convex_mesh);

    // GeometryData ch_geom_data = ConvexHull::Generate(mesh->Vertices);
    // auto *polyhedral_mesh = GeometryDataToPolyhedronMesh(ch_geom_data);
    // auto *shape = PhysicsCommon.createConvexMeshShape(polyhedral_mesh);

    // Just using a bounding box for now.
    // Need to add solid mesh debugging capabilities in this app, so I can reason better about meshes.
    // Need:
    //   - View the convex hull of any mesh.
    //   - View the bounding box of any mesh.
    //   - View normals as arrows.
    //   - Render normals as colors.
    //   - View vertex indices.
    //   - Count triangle faces.
    //   - Implement true flat shading by duplicating vertices and storing face normals (rather than vertex normals).
    //   - Store & render as polyhedral faces with arbitrary vertices per face, and mixed face types.
    // auto [geom_min, geom_max] = mesh->ComputeBounds();
    // auto *shape = PhysicsCommon.createBoxShape(Glm2Rp3d((geom_max - geom_min) * 0.5f));

    auto *body = World->createRigidBody(Glm2Rp3d(mesh->GetTransform()));
    body->setMass(1.f);
    body->addCollider(shape, {});
    body->setIsAllowedToSleep(false);
    RigidBodies.push_back({body, mesh});
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
