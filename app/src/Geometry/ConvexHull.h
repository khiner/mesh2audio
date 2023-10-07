#pragma once

#include "QuickHull.hpp"

#include "GeometryData.h"

struct ConvexHull {
    inline static GeometryData ToGeometryData(const quickhull::ConvexHull<float> &hull) {
        const auto &vertices = hull.getVertexBuffer();
        const auto &indices = hull.getIndexBuffer();
        GeometryData data;
        for (const auto &vertex : vertices) data.Vertices.emplace_back(vertex.x, vertex.y, vertex.z);
        for (size_t i = 0; i < indices.size(); i++) data.Indices.emplace_back(indices[i]);
        return data;
    }

    inline static GeometryData Generate(const std::vector<glm::vec3> &points) {
        using namespace quickhull;
        static QuickHull<float> qh{}; // Could be double as well.

        return ToGeometryData(qh.getConvexHull(&points[0][0], points.size(), true, false));
    }
};
