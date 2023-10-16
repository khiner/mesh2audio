#include "Cuboid.h"

Cuboid::Cuboid(glm::vec3 half_extents) : Geometry() {
    std::vector<Point> vertices = {
        {-half_extents.x, -half_extents.y, -half_extents.z},
        {half_extents.x, -half_extents.y, -half_extents.z},
        {half_extents.x, half_extents.y, -half_extents.z},
        {-half_extents.x, half_extents.y, -half_extents.z},
        {-half_extents.x, -half_extents.y, half_extents.z},
        {half_extents.x, -half_extents.y, half_extents.z},
        {half_extents.x, half_extents.y, half_extents.z},
        {-half_extents.x, half_extents.y, half_extents.z}};

    std::vector<VH> vhs;
    for (const auto &vertex : vertices) vhs.push_back(Mesh.add_vertex(vertex));

    std::vector<std::vector<VH>> faces = {
        {vhs[0], vhs[3], vhs[2], vhs[1]}, // front
        {vhs[4], vhs[5], vhs[6], vhs[7]}, // back
        {vhs[0], vhs[1], vhs[5], vhs[4]}, // bottom
        {vhs[3], vhs[7], vhs[6], vhs[2]}, // top
        {vhs[0], vhs[4], vhs[7], vhs[3]}, // left
        {vhs[1], vhs[2], vhs[6], vhs[5]} // right
    };

    for (const auto &face : faces) Mesh.add_face(face);

    UpdateBuffersFromMesh();
}
