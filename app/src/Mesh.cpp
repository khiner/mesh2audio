#include "Mesh.h"

#include <fstream>
#include <iomanip>
#include <thread>

// mesh2faust/vega
#include "mesh2faust.h"
#include "tetMesher.h"

#include "imspinner.h"

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <cinolib/meshes/meshes.h>
#include <cinolib/tetgen_wrap.h>

#include "Shader.h"

#include <fmt/chrono.h>
// auto start = std::chrono::high_resolution_clock::now();
// auto end = std::chrono::high_resolution_clock::now();
// auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
// std::cout << fmt::format("{}", duration) << std::endl;

using std::string, std::to_string;

// Alias for epoch seconds.
using seconds_t = std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>;

static std::thread GeneratorThread; // Worker thread for generating tetrahedral meshes.

static const vec3 Origin{0.f}, Up{0.f, 1.f, 0.f};

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
    CameraView = glm::lookAt(eye * CameraDistance, Origin, Up);

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
    if (is_svg) {
        Profile = std::make_unique<MeshProfile>(FilePath);
        ExtrudeProfile();
        return;
    }

    FILE *fp;
    fp = fopen(FilePath.c_str(), "rb");
    if (fp == nullptr) throw std::runtime_error("Error loading file: " + FilePath.string());

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
            TriangularMesh.Vertices.push_back({x, y, z});
        } else if ((c1 == 'v') && (c2 == 'n')) {
            fscanf(fp, "%f %f %f", &x, &y, &z);
            TriangularMesh.Normals.push_back(glm::normalize(vec3(x, y, z)));
        } else if (c1 == 'f') {
            fscanf(fp, "%d//%d %d//%d %d//%d", &fx, &ignore, &fy, &ignore, &fz, &ignore);
            TriangularMesh.Indices.push_back(fx - 1);
            TriangularMesh.Indices.push_back(fy - 1);
            TriangularMesh.Indices.push_back(fz - 1);
        }
    }
    fclose(fp);

    TriangularMesh.UpdateBounds();
    TriangularMesh.Center();
    Bind();
}

Mesh::~Mesh() {
    glDeleteBuffers(1, &IndexBuffer);
    glDeleteBuffers(1, &NormalBuffer);
    glDeleteBuffers(1, &VertexBuffer);
    glDeleteVertexArrays(1, &VertexArray);

    if (GeneratorThread.joinable()) GeneratorThread.join();
}

const Mesh::Data &Mesh::GetActiveData() const {
    if (ActiveViewMeshType == MeshType_Triangular) return TriangularMesh;
    return TetMesh;
}

void Mesh::Save(fs::path file_path) const {
    const auto &data = GetActiveData();
    data.Save(file_path);
}

void Mesh::Flip(bool x, bool y, bool z) {
    TriangularMesh.Flip(x, y, z);
    TetMesh.Flip(x, y, z);
    Bind();
}
void Mesh::Rotate(const vec3 &axis, float angle) {
    TriangularMesh.Rotate(axis, angle);
    TetMesh.Rotate(axis, angle);
    Bind();
}
void Mesh::Scale(const vec3 &scale) {
    TriangularMesh.Scale(scale);
    TetMesh.Scale(scale);
    Bind();
}
void Mesh::Center() {
    TriangularMesh.Center();
    TetMesh.Center();
    Bind();
}
void Mesh::ExtrudeProfile() {
    if (Profile == nullptr) return;

    TetMesh.Clear();
    TetMeshPath.clear();
    TriangularMesh.ExtrudeProfile(*Profile);
    ActiveViewMeshType = MeshType_Triangular;
    Bind();
}

void Mesh::Data::Clear() {
    Vertices.clear();
    Normals.clear();
    Indices.clear();
    Min = {};
    Max = {};
}
void Mesh::Data::Save(fs::path file_path) const {
    std::ofstream out(file_path.c_str());
    if (!out.is_open()) throw std::runtime_error(string("Error opening file: ") + file_path.string());

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
void Mesh::Data::Flip(bool x, bool y, bool z) {
    const vec3 flip(x ? -1 : 1, y ? -1 : 1, z ? -1 : 1);
    const vec3 center = (Min + Max) / 2.0f;
    for (auto &vertex : Vertices) vertex = center + (vertex - center) * flip;
    for (auto &normal : Normals) normal *= flip;
    UpdateBounds();
}
void Mesh::Data::Rotate(const vec3 &axis, float angle) {
    const glm::qua rotation = glm::angleAxis(glm::radians(angle), glm::normalize(axis));
    for (auto &vertex : Vertices) vertex = rotation * vertex;
    for (auto &normal : Normals) normal = rotation * normal;
    UpdateBounds();
}
void Mesh::Data::Scale(const vec3 &scale) {
    for (auto &vertex : Vertices) vertex *= scale;
    UpdateBounds();
}
void Mesh::Data::Center() {
    const vec3 center = (Min + Max) / 2.0f;
    for (auto &vertex : Vertices) vertex -= center;
    UpdateBounds();
}
void Mesh::Data::UpdateBounds() {
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
void Mesh::Data::ExtrudeProfile(const MeshProfile &profile) {
    Clear();
    if (profile.NumVertices() < 3) return;

    // The profile vertices are ordered clockwise, with the first vertex corresponding to the top/outside of the surface,
    // and last vertex corresponding the the bottom/inside of the surface.
    // If the profile is not closed (default), these top/bottom vertices will be connected in the middle of the extruded 3D mesh,
    // creating a continuous connected solid "bridge" between all rotated slices.
    const vector<ImVec2> &profile_vertices = profile.GetVertices();
    const int slices = profile.NumRadialSlices;
    const bool is_closed = profile.IsClosed();
    const int profile_size = profile_vertices.size();
    const int start_index = is_closed ? 0 : 1;
    const int end_index = profile_size - (is_closed ? 0 : 1);
    const int profile_size_no_connect = end_index - start_index;
    const int num_vertices = slices * profile_size_no_connect + (is_closed ? 0 : 2);
    const int num_indices = slices * (profile_size_no_connect + (is_closed ? -1 : 0)) * 6;
    Vertices.reserve(num_vertices);
    Normals.reserve(num_vertices);
    Indices.reserve(num_indices);

    const double angle_increment = 2.0 * M_PI / slices;
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
    CameraView = glm::lookAt(eye * (distance / CameraDistance), Origin, Up);
    CameraDistance = distance;
}
void Mesh::UpdateCameraProjection(const ImVec2 &size) {
    CameraProjection = glm::perspective(glm::radians(fov * 2), size.x / size.y, 0.1f, 100.f);
}

void Mesh::Render() {
    if (ViewMeshType == MeshType_Tetrahedral && !HasTetMesh()) {
        ViewMeshType = MeshType_Triangular;
    }
    if (ActiveViewMeshType != ViewMeshType) {
        ActiveViewMeshType = ViewMeshType;
        Bind();
    }

    glUniformMatrix4fv(projectionPos, 1, GL_FALSE, &CameraProjection[0][0]);
    glUniformMatrix4fv(modelviewPos, 1, GL_FALSE, &(CameraView * ObjectMatrix)[0][0]);

    glUniform4fv(lightpos, NumLights, LightPositions);
    glUniform4fv(lightcol, NumLights, LightColors);

    glUniform4fv(ambientcol, 1, Ambient);
    glUniform4fv(diffusecol, 1, Diffusion);
    glUniform4fv(specularcol, 1, Specular);
    glUniform1f(shininesscol, Shininess);

    const auto &data = GetActiveData();
    const int num_indices = data.Indices.size();
    glBindVertexArray(VertexArray);
    if (RenderMode == 0) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDrawElements(GL_TRIANGLES, num_indices, GL_UNSIGNED_INT, 0);
    }
    if (RenderMode == 1) {
        glLineWidth(1);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDrawElements(GL_TRIANGLES, num_indices, GL_UNSIGNED_INT, 0);
    }
    if (RenderMode == 2) {
        glPointSize(2.5);
        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
        glDrawElements(GL_TRIANGLES, num_indices, GL_UNSIGNED_INT, 0);
    }
    if (RenderMode == 3) {
        const static GLfloat black[4] = {0, 0, 0, 0}, white[4] = {1, 1, 1, 1};

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDrawElements(GL_TRIANGLES, num_indices, GL_UNSIGNED_INT, 0);
        glUniform4fv(diffusecol, 1, black);
        glUniform4fv(specularcol, 1, white);

        glPointSize(2.5);
        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
        glDrawElements(GL_TRIANGLES, num_indices, GL_UNSIGNED_INT, 0);

        glLineWidth(2.5);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDrawElements(GL_TRIANGLES, num_indices, GL_UNSIGNED_INT, 0);
    }
    glBindVertexArray(0);
}

static const fs::path TempDir = "tmp";
static const fs::path TetSaveDir = "saved_tet_meshes";
static const int MaxSavedTetMeshes = 8;

// Store flat tet indices, to convert to Vega tetmesh later.
static vector<uint> tet_indices;

static string FormatTime(seconds_t seconds) {
    return fmt::format("{:%m-%d %H:%M:%S %Z}", seconds);
}

static seconds_t GetTimeFromPath(const fs::path &file_path) {
    const string &filename = file_path.stem(); // Name without extension.
    return seconds_t{std::chrono::seconds(std::stoi(filename))};
}

string Mesh::GetTetMeshName(fs::path file_path) {
    return FormatTime(GetTimeFromPath(file_path));
}

static vector<seconds_t> GetSavedTetMeshTimes() {
    vector<seconds_t> saved_tet_mesh_times;
    for (const auto &entry : fs::directory_iterator(TetSaveDir)) {
        saved_tet_mesh_times.push_back(GetTimeFromPath(entry.path()));
    }
    // Sort by most recent first.
    std::sort(saved_tet_mesh_times.begin(), saved_tet_mesh_times.end(), std::greater<>());
    return saved_tet_mesh_times;
}

void Mesh::LoadTetMesh(fs::path file_path, const vector<cinolib::vec3d> &tet_vecs, const vector<vector<uint>> &tet_polys) {
    const cinolib::Tetmesh tet_mesh{tet_vecs, tet_polys};
    TetMesh.Clear();
    TetMesh.Vertices.reserve(tet_vecs.size());
    TetMesh.Normals.reserve(tet_vecs.size());
    TetMesh.Indices.reserve(tet_mesh.num_faces() * 3);

    // Bind vertices, normals, and indices to the tetrahedral mesh.
    for (size_t i = 0; i < tet_vecs.size(); i++) {
        const auto &v = tet_vecs[i];
        const float x = v[0], y = v[1], z = v[2];
        const float angle = atan2(z, x);
        TetMesh.Vertices.push_back({x, y, z});
        TetMesh.Normals.push_back({cos(angle), 0, sin(angle)});
        // const auto &normal = tet_mesh.face_data(fid).normal;
        // Normals.push_back({normal.x(), normal.y(), normal.z()});
    }
    for (uint fid = 0; fid < tet_mesh.num_faces(); ++fid) {
        const auto &tes = tet_mesh.face_tessellation(fid);
        for (uint i = 0; i < tes.size(); ++i) {
            TetMesh.Indices.push_back(tes.at(i));
        }
    }
    TetMesh.UpdateBounds();
    TetMesh.Center();
    TetMeshPath = file_path;
    ViewMeshType = MeshType_Tetrahedral;
    ActiveViewMeshType = MeshType_Triangular; // Force an automatic rebind on the next render (xxx crappy way of doing this).
}

void Mesh::LoadTetMesh(fs::path file_path) {
    vector<cinolib::vec3d> tet_vecs;
    vector<vector<uint>> tet_polys;
    read_MESH(file_path.c_str(), tet_vecs, tet_polys);
    LoadTetMesh(file_path, tet_vecs, tet_polys);
}

void Mesh::GenerateTetMesh() {
    static vector<cinolib::vec3d> tet_vecs;
    // Write to an obj file and read back into a cinolib tetmesh.
    fs::create_directory(TempDir); // Create the temp dir if it doesn't exist.
    const fs::path tmp_obj_path = TempDir / "tmp.obj";
    Save(tmp_obj_path);
    cinolib::Polygonmesh<> poly_mesh(tmp_obj_path.c_str());
    vector<uint> edges_in; // Not used.
    tetgen_wrap(poly_mesh.vector_verts(), poly_mesh.vector_polys(), edges_in, "q", tet_vecs, tet_indices);
    fs::remove(tmp_obj_path); // Delete the temporary file.

    // Write the tet mesh to disk, deleting the oldest tet mesh(es) if this puts us past the max save limit.
    fs::create_directory(TetSaveDir);
    const auto &saved_tet_mesh_times = GetSavedTetMeshTimes();
    for (int delete_i = saved_tet_mesh_times.size() - 1; delete_i >= MaxSavedTetMeshes - 1; delete_i--) {
        const seconds_t oldest_time = saved_tet_mesh_times.at(delete_i);
        fs::remove(TetSaveDir / (to_string(oldest_time.time_since_epoch().count()) + ".mesh"));
    }
    const int epoch_seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const fs::path file_path = TetSaveDir / (to_string(epoch_seconds) + ".mesh");
    const auto &tet_polys = cinolib::polys_from_serialized_vids(tet_indices, 4);
    write_MESH(file_path.c_str(), tet_vecs, tet_polys);

    LoadTetMesh(file_path, tet_vecs, tet_polys);
}

void Mesh::Bind() {
    const auto &data = GetActiveData();
    Bind(data);
}

void Mesh::Bind(const Mesh::Data &data) {
    glGenVertexArrays(1, &VertexArray);
    glGenBuffers(1, &VertexBuffer);
    glGenBuffers(1, &NormalBuffer);
    glGenBuffers(1, &IndexBuffer);

    glBindVertexArray(VertexArray);

    // Bind vertices to layout location 0
    glBindBuffer(GL_ARRAY_BUFFER, VertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * data.Vertices.size(), &data.Vertices[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); // This allows usage of layout location 0 in the vertex shader
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), 0);

    // Bind normals to layout location 1
    glBindBuffer(GL_ARRAY_BUFFER, NormalBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * data.Normals.size(), &data.Normals[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(1); // This allows usage of layout location 1 in the vertex shader
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * data.Indices.size(), &data.Indices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

string Mesh::GenerateDsp() const {
    if (!HasTetMesh()) return ""; // Tet mesh must be generated first.

    // This is a copy of `tet_vecs`, but using raw doubles, since there is no matching `tetgen_wrap` method.
    static vector<double> tet_vertices;
    const auto &tet_vecs = TetMesh.Vertices;
    tet_vertices.resize(tet_vecs.size() * 3);
    for (size_t i = 0; i < tet_vecs.size(); i++) {
        tet_vertices[i * 3 + 0] = tet_vecs[i].x;
        tet_vertices[i * 3 + 1] = tet_vecs[i].y;
        tet_vertices[i * 3 + 2] = tet_vecs[i].z;
    }

    // Convert the tetrahedram mesh data into a VegaFEM tetmesh.
    // We do this as a one-off every time, so that this is the only method that needs to be aware of VegaFEM types.
    ::TetMesh volumetric_mesh{
        int(tet_vecs.size()), tet_vertices.data(), int(tet_indices.size() / 4), (int *)tet_indices.data(),
        Material.YoungModulus, Material.PoissonRatio, Material.Density};

    return m2f::mesh2faust(
        &volumetric_mesh,
        "modalModel", // generated object name
        false, // freq control activated
        20, // lowest mode freq
        10000, // highest mode freq
        20, // number of synthesized modes (default is 20)
        50, // number of modes to be computed for the finite element analysis (default is 100)
        {}, // specific excitation positions
        10 // number of excitation positions (default is max: -1)
    );
}

static string GenerateTetMsg = "Generating tetrahedral mesh...";

using namespace ImGui;

void Mesh::RenderConfig() {
    if (BeginTabBar("MeshConfigTabBar")) {
        if (BeginTabItem("Mesh")) {
            const auto &center = GetMainViewport()->GetCenter();
            const auto &popup_size = GetMainViewport()->Size / 4;
            SetNextWindowPos(center, ImGuiCond_Appearing, {0.5f, 0.5f});
            SetNextWindowSize(popup_size);
            if (BeginPopupModal(GenerateTetMsg.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                const auto &ws = GetWindowSize();
                const float spinner_dim = std::min(ws.x, ws.y) / 2;
                SetCursorPos((ws - ImVec2{spinner_dim, spinner_dim}) / 2 + ImVec2(0, GetTextLineHeight()));
                ImSpinner::SpinnerMultiFadeDots(GenerateTetMsg.c_str(), spinner_dim / 2, 3);
                if (HasTetMesh()) CloseCurrentPopup();
                EndPopup();
            }
            Text("File: %s", FilePath.c_str());
            if (HasTetMesh()) {
                const string name = GetTetMeshName(TetMeshPath);
                Text(
                    "Current tetrahedral mesh:\n\tGenerated: %s\n\tVertices: %lu\n\tIndices: %lu",
                    name.c_str(), TetMesh.Vertices.size(), TetMesh.Indices.size()
                );
                TextUnformatted("View mesh type");
                RadioButton("Triangular", &ViewMeshType, MeshType_Triangular);
                SameLine();
                RadioButton("Tetrahedral", &ViewMeshType, MeshType_Tetrahedral);
            } else {
                TextUnformatted("No tetrahedral mesh loaded");
                if (Button("Generate tetrahedral mesh")) {
                    OpenPopup(GenerateTetMsg.c_str());
                    if (GeneratorThread.joinable()) GeneratorThread.join();
                    GeneratorThread = std::thread([&] { GenerateTetMesh(); });
                }
            }
            // Dropdown to select from saved tet meshes and load.
            if (BeginCombo("Saved tet meshes", nullptr)) {
                const auto &saved_tet_mesh_times = GetSavedTetMeshTimes();
                if (saved_tet_mesh_times.empty()) TextUnformatted("No saved tet meshes");
                for (const seconds_t tet_mesh_time : saved_tet_mesh_times) {
                    const string formatted_time = FormatTime(tet_mesh_time);
                    if (Selectable(formatted_time.c_str(), false)) {
                        LoadTetMesh(TetSaveDir / (to_string(tet_mesh_time.time_since_epoch().count()) + ".mesh"));
                    }
                }
                EndCombo();
            }

            SeparatorText("Modify");
            if (Button("Center")) Center();
            Text("Flip");
            SameLine();
            if (Button("X##Flip")) Flip(true, false, false);
            SameLine();
            if (Button("Y##Flip")) Flip(false, true, false);
            SameLine();
            if (Button("Z##Flip")) Flip(false, false, true);

            Text("Rotate 90 deg.");
            SameLine();
            if (Button("X##Rotate")) Rotate({1, 0, 0}, 90);
            SameLine();
            if (Button("Y##Rotate")) Rotate({0, 1, 0}, 90);
            SameLine();
            if (Button("Z##Rotate")) Rotate({0, 0, 1}, 90);

            SeparatorText("Render mode");
            RadioButton("Smooth", &RenderMode, RenderType_Smooth);
            SameLine();
            RadioButton("Lines", &RenderMode, RenderType_Lines);
            RadioButton("Point cloud", &RenderMode, RenderType_Points);
            SameLine();
            RadioButton("Mesh", &RenderMode, RenderType_Mesh);
            NewLine();
            SeparatorText("Gizmo");
            Checkbox("Show gizmo", &ShowGizmo);
            if (ShowGizmo) {
                const string interaction_text = "Interaction: " +
                    string(ImGuizmo::IsUsing() ? "Using Gizmo" : ImGuizmo::IsOver(ImGuizmo::TRANSLATE) ? "Translate hovered" :
                               ImGuizmo::IsOver(ImGuizmo::ROTATE)                                      ? "Rotate hovered" :
                               ImGuizmo::IsOver(ImGuizmo::SCALE)                                       ? "Scale hovered" :
                               ImGuizmo::IsOver()                                                      ? "Hovered" :
                                                                                                         "Not interacting");
                TextUnformatted(interaction_text.c_str());

                if (IsKeyPressed(ImGuiKey_T)) GizmoOp = ImGuizmo::TRANSLATE;
                if (IsKeyPressed(ImGuiKey_R)) GizmoOp = ImGuizmo::ROTATE;
                if (IsKeyPressed(ImGuiKey_S)) GizmoOp = ImGuizmo::SCALE;
                if (RadioButton("Translate (T)", GizmoOp == ImGuizmo::TRANSLATE)) GizmoOp = ImGuizmo::TRANSLATE;
                if (RadioButton("Rotate (R)", GizmoOp == ImGuizmo::ROTATE)) GizmoOp = ImGuizmo::ROTATE;
                if (RadioButton("Scale (S)", GizmoOp == ImGuizmo::SCALE)) GizmoOp = ImGuizmo::SCALE;
                if (RadioButton("Universal", GizmoOp == ImGuizmo::UNIVERSAL)) GizmoOp = ImGuizmo::UNIVERSAL;
                Checkbox("Bound sizing", &ShowBounds);
            }
            EndTabItem();
        }
        if (BeginTabItem("Mesh profile")) {
            RenderProfileConfig();
            EndTabItem();
        }
        if (BeginTabItem("Camera")) {
            Checkbox("Show gizmo", &ShowCameraGizmo);
            SameLine();
            Checkbox("Grid", &ShowGrid);
            SliderFloat("FOV", &fov, 20.f, 110.f);

            float camera_distance = CameraDistance;
            if (SliderFloat("Distance", &camera_distance, .1f, 10.f)) {
                SetCameraDistance(camera_distance);
            }
            EndTabItem();
        }
        if (BeginTabItem("Lighing")) {
            SeparatorText("Colors");
            Checkbox("Custom colors", &CustomColor);
            if (CustomColor) {
                ColorEdit3("Ambient", &Ambient[0]);
                ColorEdit3("Diffusion", &Diffusion[0]);
                ColorEdit3("Specular", &Specular[0]);
                SliderFloat("Shininess", &Shininess, 0.0f, 150.0f);
            } else {
                for (int i = 1; i < 3; i++) {
                    Ambient[i] = Ambient[0];
                    Diffusion[i] = Diffusion[0];
                    Specular[i] = Specular[0];
                }
                SliderFloat("Ambient", &Ambient[0], 0.0f, 1.0f);
                SliderFloat("Diffusion", &Diffusion[0], 0.0f, 1.0f);
                SliderFloat("Specular", &Specular[0], 0.0f, 1.0f);
                SliderFloat("Shininess", &Shininess, 0.0f, 150.0f);
            }

            SeparatorText("Lights");
            for (int i = 0; i < NumLights; i++) {
                Separator();
                PushID(i);
                Text("Light %d", i + 1);
                SliderFloat3("Positions", &LightPositions[4 * i], -25.0f, 25.0f);
                ColorEdit3("Color", &LightColors[4 * i]);
                PopID();
            }
            EndTabItem();
        }
        EndTabBar();
    }
}

void Mesh::RenderProfile() {
    if (Profile == nullptr) Text("The current mesh was not loaded from a 2D profile.");
    else if (Profile->Render()) ExtrudeProfile();
}

void Mesh::RenderProfileConfig() {
    if (Profile == nullptr) Text("The current mesh was not loaded from a 2D profile.");
    else if (Profile->RenderConfig()) ExtrudeProfile();
}
