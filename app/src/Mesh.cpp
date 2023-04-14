#include "Mesh.h"

#include <iomanip>

// mesh2faust/vega
#include "mesh2faust.h"
#include "tetMesher.h"

#include <fstream>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "Shader.h"

// #include <fmt/chrono.h>
// auto start = std::chrono::high_resolution_clock::now();
// auto end = std::chrono::high_resolution_clock::now();
// auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
// std::cout << fmt::format("{}", duration) << std::endl;

static const vec3 center = {0.f, 0.f, 0.f}, up = {0.f, 1.f, 0.f};

static bool StaticInitialized = false;
static GLuint projectionPos, modelviewPos;
// Variables to set uniform params for lighting fragment shader
static GLuint lightcol, lightpos, ambientcol, diffusecol, specularcol, emissioncol, shininesscol;

void Mesh::InitializeStatic() {
    if (StaticInitialized) return;

    // Initialize all colors to white, and initialize the light positions to be in a circle on the xz plane.
    std::fill_n(LightColors, NumLights * 4, 1.0f);
    for (int i = 0; i < NumLights; i++) {
        const float angle = 2 * M_PI * i / NumLights;
        const float dist = 15.0f;
        LightPositions[i * 4 + 0] = dist * cosf(angle);
        LightPositions[i * 4 + 1] = 0;
        LightPositions[i * 4 + 2] = dist * sinf(angle);
        LightPositions[4 * i + 3] = 1.0f;
        LightColors[4 * i + 3] = 1.0f;
    }

    /**
      Initialize a right-handed coordinate system, with:
        * Positive x pointing right
        * Positive y pointing up, and
        * Positive z pointing forward (toward the camera).
      This would put the camera `eye` at position (0, 0, camDistance) in world space, pointing at the origin.
      We offset the camera angle slightly from this point along spherical coordinates to make the initial view more interesting.
    */
    static float x_angle = M_PI / 10; // Elevation angle (0° is in the X-Z plane, positive angles rotate upwards)
    static float y_angle = M_PI / 2 - M_PI / 10; // Azimuth angle (0° is along +X axis, positive angles rotate counterclockwise)
    static vec3 eye(cosf(y_angle) * cosf(x_angle), sinf(x_angle), sinf(y_angle) * cosf(x_angle));
    CameraView = glm::lookAt(eye * CameraDistance, center, up);

    static GLuint vertexshader = Shader::InitShader(GL_VERTEX_SHADER, fs::path("res") / "shaders" / "vertex.glsl");
    static GLuint fragmentshader = Shader::InitShader(GL_FRAGMENT_SHADER, fs::path("res") / "shaders" / "fragment.glsl");
    static GLuint shaderprogram = Shader::InitProgram(vertexshader, fragmentshader);

    lightpos = glGetUniformLocation(shaderprogram, "light_posn");
    lightcol = glGetUniformLocation(shaderprogram, "light_col");
    ambientcol = glGetUniformLocation(shaderprogram, "ambient");
    diffusecol = glGetUniformLocation(shaderprogram, "diffuse");
    specularcol = glGetUniformLocation(shaderprogram, "specular");
    emissioncol = glGetUniformLocation(shaderprogram, "emission");
    shininesscol = glGetUniformLocation(shaderprogram, "shininess");
    projectionPos = glGetUniformLocation(shaderprogram, "projection");
    modelviewPos = glGetUniformLocation(shaderprogram, "modelview");

    StaticInitialized = true;
}

Mesh::Mesh(fs::path file_path) {
    InitializeStatic();

    const bool is_svg = file_path.extension() == ".svg";
    const bool is_obj = file_path.extension() == ".obj";
    if (!is_svg && !is_obj) throw std::runtime_error("Unsupported file type: " + file_path.string());

    FilePath = file_path; // Store the most recent file path.

    glGenVertexArrays(1, &VertexArray);
    glGenBuffers(1, &VertexBuffer);
    glGenBuffers(1, &NormalBuffer);
    glGenBuffers(1, &IndexBuffer);

    if (is_svg) {
        Profile = std::make_unique<MeshProfile>(file_path);
        ExtrudeProfile();
        return;
    }

    VolumetricMesh.reset();
    Profile.reset();

    FILE *fp;
    fp = fopen(file_path.c_str(), "rb");
    if (fp == nullptr) throw std::runtime_error("Error loading file: " + file_path.string());

    float x, y, z;
    int fx, fy, fz, ignore;
    int c1, c2;
    while (!feof(fp)) {
        c1 = fgetc(fp);
        while (!(c1 == 'v' || c1 == 'f')) {
            c1 = fgetc(fp);
            if (feof(fp)) break;
        }
        c2 = fgetc(fp);
        if ((c1 == 'v') && (c2 == ' ')) {
            fscanf(fp, "%f %f %f", &x, &y, &z);
            Vertices.push_back({x, y, z});
        } else if ((c1 == 'v') && (c2 == 'n')) {
            fscanf(fp, "%f %f %f", &x, &y, &z);
            Normals.push_back(glm::normalize(vec3(x, y, z)));
        } else if (c1 == 'f') {
            fscanf(fp, "%d//%d %d//%d %d//%d", &fx, &ignore, &fy, &ignore, &fz, &ignore);
            Indices.push_back(fx - 1);
            Indices.push_back(fy - 1);
            Indices.push_back(fz - 1);
        }
    }
    fclose(fp);

    UpdateBounds();
    Center();
}

Mesh::~Mesh() {
    glDeleteBuffers(1, &IndexBuffer);
    glDeleteBuffers(1, &NormalBuffer);
    glDeleteBuffers(1, &VertexBuffer);
    glDeleteVertexArrays(1, &VertexArray);
}

void Mesh::Save(fs::path file_path) const {
    std::ofstream out(file_path.c_str());
    if (!out.is_open()) throw std::runtime_error(std::string("Error opening file: ") + file_path.string());

    out << std::setprecision(10);
    for (const vec3 &v : Vertices) {
        out << "v " << v.x << " " << v.y << " " << v.z << "\n";
    }
    for (const vec3 &n : Normals) {
        out << "vn " << n.x << " " << n.y << " " << n.z << "\n";
    }
    for (size_t i = 0; i < Indices.size(); i += 3) {
        out << "f " << Indices[i] + 1 << "//" << Indices[i] + 1 << " "
            << Indices[i + 1] + 1 << "//" << Indices[i + 1] + 1 << " "
            << Indices[i + 2] + 1 << "//" << Indices[i + 2] + 1 << "\n";
    }

    out.close();
}

void Mesh::UpdateBounds() {
    // Update `Min`/`Max`, the bounds of the mesh, based on the current vertices.
    Min = vec3(INFINITY, INFINITY, INFINITY);
    Max = vec3(-INFINITY, -INFINITY, -INFINITY);
    for (const vec3 &v : Vertices) {
        if (v.x < Min.x) Min.x = v.x;
        if (v.y < Min.y) Min.y = v.y;
        if (v.z < Min.z) Min.z = v.z;
        if (v.x > Max.x) Max.x = v.x;
        if (v.y > Max.y) Max.y = v.y;
        if (v.z > Max.z) Max.z = v.z;
    }
}

void Mesh::Bind() {
    UpdateBounds();

    glBindVertexArray(VertexArray);

    // Bind vertices to layout location 0
    glBindBuffer(GL_ARRAY_BUFFER, VertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * Vertices.size(), &Vertices[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); // This allows usage of layout location 0 in the vertex shader
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), 0);

    // Bind normals to layout location 1
    glBindBuffer(GL_ARRAY_BUFFER, NormalBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * Normals.size(), &Normals[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(1); // This allows usage of layout location 1 in the vertex shader
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * Indices.size(), &Indices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Mesh::Flip(bool x, bool y, bool z) {
    const vec3 flip(x ? -1 : 1, y ? -1 : 1, z ? -1 : 1);
    const vec3 center = (Min + Max) / 2.0f;
    for (auto &vertex : Vertices) vertex = center + (vertex - center) * flip;
    for (auto &normal : Normals) normal *= flip;
    Bind();
}
void Mesh::Rotate(const vec3 &axis, float angle) {
    const glm::qua rotation = glm::angleAxis(glm::radians(angle), glm::normalize(axis));
    for (auto &vertex : Vertices) vertex = rotation * vertex;
    for (auto &normal : Normals) normal = rotation * normal;
    Bind();
}
void Mesh::Scale(const vec3 &scale) {
    for (auto &vertex : Vertices) vertex *= scale;
    Bind();
}

void Mesh::Center() {
    const vec3 center = (Min + Max) / 2.0f;
    for (auto &vertex : Vertices) vertex -= center;
    Bind();
}

void Mesh::ExtrudeProfile() {
    if (Profile == nullptr || Profile->NumVertices() < 3) return;

    VolumetricMesh.reset();
    Vertices.clear();
    Normals.clear();
    Indices.clear();

    // The profile vertices are ordered clockwise, with the first vertex corresponding to the top/outside of the surface,
    // and last vertex corresponding the the bottom/inside of the surface.
    // If the profile is not closed (default), these top/bottom vertices will be connected in the middle of the extruded 3D mesh,
    // creating a continuous connected solid "bridge" between all rotated slices.
    const vector<ImVec2> &profile_vertices = Profile->GetVertices();
    const int profile_size = profile_vertices.size();
    const int slices = Profile->NumRadialSlices;
    const double angle_increment = 2.0 * M_PI / slices;
    const bool is_closed = Profile->IsClosed();
    const int start_index = is_closed ? 0 : 1;
    const int end_index = profile_size - (is_closed ? 0 : 1);
    for (int slice = 0; slice < slices; slice++) {
        const double angle = slice * angle_increment;
        const double c = cos(angle);
        const double s = sin(angle);
        // Exclude the top/bottom vertices, which will be connected later.
        for (int i = start_index; i < end_index; i++) {
            const auto &p = profile_vertices[i];
            Vertices.push_back({p.x * c, p.y, p.x * s});
            Normals.push_back({c, 0.0, s});
        }
    }
    if (!is_closed) {
        Vertices.push_back({0.0, profile_vertices[0].y, 0.0});
        Normals.push_back({0.0, 0.0, 0.0});
        Vertices.push_back({0.0, profile_vertices[profile_size - 1].y, 0.0});
        Normals.push_back({0.0, 0.0, 0.0});
    }

    // Compute indices for the triangles.
    const int profile_size_no_connect = end_index - start_index;
    for (int slice = 0; slice < slices; slice++) {
        for (int i = 0; i < profile_size_no_connect - 1; i++) {
            const int base_index = slice * profile_size_no_connect + i;
            const int next_base_index = ((slice + 1) % slices) * profile_size_no_connect + i;
            // First triangle
            Indices.push_back(base_index);
            Indices.push_back(next_base_index + 1);
            Indices.push_back(base_index + 1);

            // Second triangle
            Indices.push_back(base_index);
            Indices.push_back(next_base_index);
            Indices.push_back(next_base_index + 1);
        }
    }

    // Connect the top and bottom.
    if (!is_closed) {
        for (int slice = 0; slice < slices; slice++) {
            // Top
            Indices.push_back(slice * profile_size_no_connect);
            Indices.push_back(Vertices.size() - 2);
            Indices.push_back(((slice + 1) % slices) * profile_size_no_connect);
            // Bottom
            Indices.push_back(slice * profile_size_no_connect + profile_size - 3);
            Indices.push_back(Vertices.size() - 1);
            Indices.push_back(((slice + 1) % slices) * profile_size_no_connect + profile_size - 3);
        }
    }

    // SVG coordinates are upside-down relative to our 3D rendering coordinates.
    // However, they're correctly oriented top-to-bottom for 2D ImGui rendering, so we only invert the 3D mesh - not the profile.
    UpdateBounds();
    Flip(false, true, false);
    Center();
}

void Mesh::SetCameraDistance(float distance) {
    // Extract the eye position from inverse camera view matrix and update the camera view based on the new distance.
    const vec3 eye = glm::inverse(CameraView)[3];
    CameraView = glm::lookAt(eye * (distance / CameraDistance), center, up);
    CameraDistance = distance;
}
void Mesh::UpdateCameraProjection(const ImVec2 &size) {
    CameraProjection = glm::perspective(glm::radians(fov * 2), size.x / size.y, 0.1f, 100.f);
}

void Mesh::Render(int mode) const {
    glUniformMatrix4fv(projectionPos, 1, GL_FALSE, &CameraProjection[0][0]);
    glUniformMatrix4fv(modelviewPos, 1, GL_FALSE, &(CameraView * ObjectMatrix)[0][0]);

    glUniform4fv(lightpos, NumLights, LightPositions);
    glUniform4fv(lightcol, NumLights, LightColors);

    glUniform4fv(ambientcol, 1, Ambient);
    glUniform4fv(diffusecol, 1, Diffusion);
    glUniform4fv(specularcol, 1, Specular);
    glUniform1f(shininesscol, Shininess);

    glBindVertexArray(VertexArray);
    if (mode == 0) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDrawElements(GL_TRIANGLES, NumIndices(), GL_UNSIGNED_INT, 0);
    }
    if (mode == 1) {
        glLineWidth(1);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDrawElements(GL_TRIANGLES, NumIndices(), GL_UNSIGNED_INT, 0);
    }
    if (mode == 2) {
        glPointSize(2.5);
        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
        glDrawElements(GL_TRIANGLES, NumIndices(), GL_UNSIGNED_INT, 0);
    }
    if (mode == 3) {
        const static GLfloat black[4] = {0, 0, 0, 0}, white[4] = {1, 1, 1, 1};

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDrawElements(GL_TRIANGLES, NumIndices(), GL_UNSIGNED_INT, 0);
        glUniform4fv(diffusecol, 1, black);
        glUniform4fv(specularcol, 1, white);

        glPointSize(2.5);
        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
        glDrawElements(GL_TRIANGLES, NumIndices(), GL_UNSIGNED_INT, 0);

        glLineWidth(2.5);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDrawElements(GL_TRIANGLES, NumIndices(), GL_UNSIGNED_INT, 0);
    }
    glBindVertexArray(0);
}

void Mesh::RenderProfile() {
    if (Profile == nullptr) ImGui::Text("The current mesh was not loaded from a 2D profile.");
    else if (Profile->Render()) ExtrudeProfile();
}

void Mesh::RenderProfileConfig() {
    if (Profile == nullptr) ImGui::Text("The current mesh was not loaded from a 2D profile.");
    else if (Profile->RenderConfig()) ExtrudeProfile();
}

// This is not working for profiles yet.
// One reason is that we normalize so that the largest dim. is 1.
// But even keeping the original scale, it just hangs.
// Using the bell .obj files works, but those are the only ones.
void Mesh::CreateTetraheralMesh() {
    // Copy the vertices/normals/indices into an ObjMesh.
    ObjMesh obj;
    obj.addGroup({"default"});
    for (const vec3 &v : Vertices) obj.addVertexPosition({v.x, v.y, v.z});
    for (const vec3 &n : Normals) obj.addVertexNormal({n.x, n.y, n.z});
    for (size_t face_i = 0; face_i < Indices.size() / 3; face_i++) {
        ObjMesh::Face face;
        for (size_t i = 0; i < 3; i++) {
            const unsigned int index = Indices[face_i * 3 + i];
            face.addVertex(ObjMesh::Vertex(index, {false, 0}, {false, index}));
        }
        obj.addFaceToGroup(face, 0);
    }
    obj.addDefaultMaterial();

    VolumetricMesh.reset(TetMesher().compute(&obj));
    const m2f::MaterialProperties material{1.05E11, 0.33, 8600};
    VolumetricMesh->setSingleMaterial(material.youngModulus, material.poissonRatio, material.density);
}

void Mesh::BindTetrahedralMesh() {
    Vertices.clear();
    Normals.clear();
    Indices.clear();

    glGenVertexArrays(1, &VertexArray);
    glGenBuffers(1, &VertexBuffer);
    glGenBuffers(1, &NormalBuffer);
    glGenBuffers(1, &IndexBuffer);

    // Bind vertices, normals, and indices to the tetrahedral mesh.
    for (int i = 0; i < VolumetricMesh->getNumVertices(); i++) {
        const auto &v = VolumetricMesh->getVertex(i);
        const float x = v[0], y = v[1], z = v[2];
        Vertices.push_back({x, y, z});
        const float angle = atan2(z, x);

        Normals.push_back({cos(angle), 0, sin(angle)});
    }
    for (int i = 0; i < VolumetricMesh->getNumElements(); i++) {
        for (int j = 0; j < VolumetricMesh->getNumElementVertices(); j++) {
            Indices.push_back(VolumetricMesh->getVertexIndex(i, j));
        }
    }

    UpdateBounds();
    Center();
}

std::string Mesh::GenerateDsp() const {
    return m2f::mesh2faust(
        VolumetricMesh.get(),
        "modalModel", // generated object name
        false, // freq control activated
        20, // lowest mode freq
        10000, // highest mode freq
        20, // number of synthesized modes
        100, // number of modes to be computed for the finite element analysis
        {}, // specific excitation positions
        -1 // number of excitation positions (default is max)
    );
}
