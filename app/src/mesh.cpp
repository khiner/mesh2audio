#include "Mesh.h"
#include "transform.h"

#include <GL/glew.h>
#include <stdexcept>

namespace gl {
void Mesh::Init() {
    glGenVertexArrays(1, &vertex_array);
    glGenBuffers(1, &vertex_buffer);
    glGenBuffers(1, &normal_buffer);
    glGenBuffers(1, &index_buffer);
}

void Mesh::Destroy() {
    glDeleteBuffers(1, &index_buffer);
    glDeleteBuffers(1, &normal_buffer);
    glDeleteBuffers(1, &vertex_buffer);
    glDeleteVertexArrays(1, &vertex_array);

    vertices.clear();
    normals.clear();
    indices.clear();
}

void Mesh::Load(fs::path object_path) {
    Destroy();
    Init();

    FILE *fp;
    fp = fopen(object_path.c_str(), "rb");
    if (fp == nullptr) throw std::runtime_error("Error loading file: " + object_path.string());

    float x, y, z;
    int fx, fy, fz, ignore;
    int c1, c2;
    float y_min = INFINITY, z_min = INFINITY;
    float y_max = -INFINITY, z_max = -INFINITY;

    while (!feof(fp)) {
        c1 = fgetc(fp);
        while (!(c1 == 'v' || c1 == 'f')) {
            c1 = fgetc(fp);
            if (feof(fp)) break;
        }
        c2 = fgetc(fp);
        if ((c1 == 'v') && (c2 == ' ')) {
            fscanf(fp, "%f %f %f", &x, &y, &z);
            vertices.push_back({x, y, z});
            if (y < y_min) y_min = y;
            if (z < z_min) z_min = z;
            if (y > y_max) y_max = y;
            if (z > z_max) z_max = z;
        } else if ((c1 == 'v') && (c2 == 'n')) {
            fscanf(fp, "%f %f %f", &x, &y, &z);
            normals.push_back(glm::normalize(vec3(x, y, z)));
        } else if (c1 == 'f') {
            fscanf(fp, "%d//%d %d//%d %d//%d", &fx, &ignore, &fy, &ignore, &fz, &ignore);
            indices.push_back(fx - 1);
            indices.push_back(fy - 1);
            indices.push_back(fz - 1);
        }
    }
    fclose(fp);

    const float y_avg = (y_min + y_max) / 2.0f - 0.02f;
    const float z_avg = (z_min + z_max) / 2.0f;
    for (unsigned int i = 0; i < vertices.size(); ++i) {
        vertices[i] -= vec3(0.0f, y_avg, z_avg);
        vertices[i] *= vec3(1.58f, 1.58f, 1.58f);
    }

    glBindVertexArray(vertex_array);

    // Bind vertices to layout location 0
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * vertices.size(), &vertices[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); // This allows usage of layout location 0 in the vertex shader
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), 0);

    // Bind normals to layout location 1
    glBindBuffer(GL_ARRAY_BUFFER, normal_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * normals.size(), &normals[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(1); // This allows usage of layout location 1 in the vertex shader
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * indices.size(), &indices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}
} // namespace gl
