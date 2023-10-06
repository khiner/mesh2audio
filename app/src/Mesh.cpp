#include "Mesh.h"

#include "date.h"
#include "mesh2faust.h"
#include "tetgen.h"

#include "Audio.h"
#include "RealImpact.h"

using glm::vec2, glm::vec3, glm::vec4, glm::mat4;
using std::string, std::to_string;
using seconds_t = std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>; // Alias for epoch seconds.

Mesh::Mesh(::Scene &scene, fs::path file_path) : Scene(scene) {
    const bool is_svg = file_path.extension() == ".svg";
    const bool is_obj = file_path.extension() == ".obj";
    if (!is_svg && !is_obj) throw std::runtime_error("Unsupported file type: " + file_path.string());

    FilePath = file_path; // Store the most recent file path.
    if (is_svg) {
        Profile = std::make_unique<MeshProfile>(FilePath);
        TriangularMesh.ExtrudeProfile(Profile->GetVertices(), Profile->NumRadialSlices, Profile->ClosePath);
    } else {
        TriangularMesh.Load(FilePath);
    }

    HoveredVertexArrow.SetColor({1, 0, 0, 1});
    UpdateExcitableVertices();
    Scene.AddGeometry(&GetActiveGeometry());
    Scene.AddGeometry(&ExcitableVertexArrows);
    Scene.AddGeometry(&HoveredVertexArrow);

    InitialBounds = TriangularMesh.ComputeBounds();
    Scene.SetCameraDistance(glm::distance(InitialBounds.first, InitialBounds.second) * 2);
}

Mesh::~Mesh() {}

void Mesh::SetViewMeshType(Type type) {
    if (ViewMeshType == type) return;

    Scene.RemoveGeometry(&GetActiveGeometry());
    ViewMeshType = type;
    Scene.AddGeometry(&GetActiveGeometry());
}

void Mesh::Save(fs::path file_path) const {
    GetActiveGeometry().Save(file_path);
}

mat4 Mesh::GetTransform() const {
    mat4 transform = Identity;
    transform = glm::translate(transform, Translation);

    mat4 rot_x = glm::rotate(Identity, glm::radians(RotationAngles.x), {1, 0, 0});
    mat4 rot_y = glm::rotate(Identity, glm::radians(RotationAngles.y), {0, 1, 0});
    mat4 rot_z = glm::rotate(Identity, glm::radians(RotationAngles.z), {0, 0, 1});
    transform *= (rot_z * rot_y * rot_x);

    return glm::scale(transform, Scale);
}

void Mesh::ApplyTransform() {
    const mat4 transform = GetTransform();
    TriangularMesh.SetTransform(transform);
    TetMesh.SetTransform(transform);
    Scene.GizmoTransform = transform;
}

void Mesh::ExtrudeProfile() {
    if (Profile == nullptr) return;

    HoveredVertexIndex = CameraTargetVertexIndex = -1;
    TetMesh.Clear();
    UpdateExcitableVertices();
    TriangularMesh.ExtrudeProfile(Profile->GetVertices(), Profile->NumRadialSlices, Profile->ClosePath);
    SetViewMeshType(MeshType_Triangular);
}

static void InterpolateColors(const float a[], const float b[], float interpolation, float result[]) {
    for (int i = 0; i < 4; i++) {
        result[i] = a[i] * (1.0 - interpolation) + b[i] * interpolation;
    }
}
static void SetColor(const float to[], float result[]) {
    for (int i = 0; i < 4; i++) result[i] = to[i];
}

void Mesh::UpdateExcitableVertexColors() {
    if (ViewMeshType == MeshType_Tetrahedral && !Audio::FaustState::Is2DModel && ShowExcitableVertices && !ExcitableVertexIndices.empty()) {
        for (size_t i = 0; i < ExcitableVertexIndices.size(); i++) {
            // todo update to use vec4 for all colors.
            static const float DisabledExcitableVertexColor[4] = {0.3, 0.3, 0.3, 1}; // For when DSP has not been initialized.
            static const float ExcitableVertexColor[4] = {1, 1, 1, 1}; // Based on `NumExcitableVertices`.
            static const float ActiveExciteVertexColor[4] = {0, 1, 0, 1}; // The most recent excited vertex.
            static const float ExcitedVertexBaseColor[4] = {1, 0, 0, 1}; // The color of the excited vertex when the gate has abs value of 1.

            static float vertex_color[4] = {1, 1, 1, 1}; // Initialized once and filled for every excitable vertex.
            if (!Audio::FaustState::IsRunning()) {
                SetColor(DisabledExcitableVertexColor, vertex_color);
            } else if (Audio::FaustState::ExcitePos != nullptr && int(i) == int(*Audio::FaustState::ExcitePos)) {
                InterpolateColors(ActiveExciteVertexColor, ExcitedVertexBaseColor, std::min(1.f, std::abs(*Audio::FaustState::ExciteValue)), vertex_color);
            } else {
                SetColor(ExcitableVertexColor, vertex_color);
            }
            ExcitableVertexArrows.Colors[i] = {vertex_color[0], vertex_color[1], vertex_color[2], vertex_color[3]};
        }
    }
}

void Mesh::UpdateExcitableVertices() {
    ExcitableVertexArrows.Transforms.clear();
    ExcitableVertexArrows.Colors.clear();
    ExcitableVertexIndices.clear();
    if (TetMesh.Vertices.empty()) return;

    // Linearly sample excitable vertices from all available vertices.
    ExcitableVertexIndices.resize(NumExcitableVertices);
    for (int i = 0; i < NumExcitableVertices; i++) {
        const float t = float(i) / (NumExcitableVertices - 1);
        ExcitableVertexIndices[i] = int(t * (TetMesh.Vertices.size() - 1));
    }

    // Point arrows at each excitable vertex.
    float scale_factor = 0.1f * glm::distance(InitialBounds.first, InitialBounds.second);
    mat4 scale = glm::scale(Identity, vec3{scale_factor});
    for (auto vertex_index : ExcitableVertexIndices) {
        mat4 translate = glm::translate(Identity, TetMesh.Vertices[vertex_index]);
        mat4 rotate = glm::toMat4(glm::rotation(Up, glm::normalize(TetMesh.Normals[vertex_index])));
        ExcitableVertexArrows.Transforms.push_back({translate * rotate * scale});
        ExcitableVertexArrows.Colors.push_back({1, 1, 1, 1});
    }
}

void Mesh::LoadRealImpact() {
    RealImpact = std::make_unique<::RealImpact>(FilePath.parent_path());
    Scene.AddGeometry(&RealImpactListenerPoints);
}

void Mesh::UpdateTetMesh() {
    if (ViewMeshType == MeshType_Tetrahedral) HoveredVertexIndex = CameraTargetVertexIndex = -1;

    TetMesh.Clear();
    for (uint i = 0; i < uint(TetGenResult->numberofpoints); ++i) {
        TetMesh.Vertices.push_back({TetGenResult->pointlist[i * 3], TetGenResult->pointlist[i * 3 + 1], TetGenResult->pointlist[i * 3 + 2]});
    }

    for (uint i = 0; i < uint(TetGenResult->numberoftrifaces); ++i) {
        const auto &tri_indices = TetGenResult->trifacelist;
        const uint tri_i = i * 3;
        // Order of triangle indices important for normal calculation.
        const uint a = tri_indices[tri_i], b = tri_indices[tri_i + 2], c = tri_indices[tri_i + 1];
        TetMesh.TriangleIndices.append({a, b, c});
    }
    TetMesh.ComputeNormals(); // todo better surface normals

    UpdateExcitableVertices();
}

void Mesh::GenerateTetMesh() {
    tetgenio in;
    in.firstnumber = 0;
    in.numberofpoints = TriangularMesh.Vertices.size();
    in.pointlist = new REAL[in.numberofpoints * 3];

    for (uint i = 0; i < TriangularMesh.Vertices.size(); ++i) {
        in.pointlist[i * 3] = TriangularMesh.Vertices[i].x;
        in.pointlist[i * 3 + 1] = TriangularMesh.Vertices[i].y;
        in.pointlist[i * 3 + 2] = TriangularMesh.Vertices[i].z;
    }

    in.numberoffacets = TriangularMesh.TriangleIndices.size() / 3;
    in.facetlist = new tetgenio::facet[in.numberoffacets];

    for (uint i = 0; i < uint(in.numberoffacets); ++i) {
        tetgenio::facet &f = in.facetlist[i];
        f.numberofpolygons = 1;
        f.polygonlist = new tetgenio::polygon[f.numberofpolygons];
        f.polygonlist[0].numberofvertices = 3;
        f.polygonlist[0].vertexlist = new int[f.polygonlist[0].numberofvertices];
        f.polygonlist[0].vertexlist[0] = TriangularMesh.TriangleIndices[i * 3];
        f.polygonlist[0].vertexlist[1] = TriangularMesh.TriangleIndices[i * 3 + 1];
        f.polygonlist[0].vertexlist[2] = TriangularMesh.TriangleIndices[i * 3 + 2];
    }

    const string options = QualityTetMesh ? "pq" : "p";
    TetGenResult = std::make_unique<tetgenio>();
    std::vector<char> options_mutable(options.begin(), options.end());
    tetrahedralize(options_mutable.data(), &in, TetGenResult.get());
}

string Mesh::GenerateDsp() const {
    if (!TetGenResult) return "";

    std::vector<int> tet_indices;
    tet_indices.reserve(TetGenResult->numberoftetrahedra * 4 * 3); // 4 triangles per tetrahedron, 3 indices per triangle.
    // Turn each tetrahedron into 4 triangles.
    for (uint i = 0; i < uint(TetGenResult->numberoftetrahedra); ++i) {
        auto &result_indices = TetGenResult->tetrahedronlist;
        uint tri_i = i * 4;
        int a = result_indices[tri_i], b = result_indices[tri_i + 1], c = result_indices[tri_i + 2], d = result_indices[tri_i + 3];
        tet_indices.insert(tet_indices.end(), {a, b, c, d, a, b, c, d, a, b, c, d});
    }
    // Convert the tetrahedral mesh into a VegaFEM tetmesh.
    ::TetMesh volumetric_mesh{
        TetGenResult->numberofpoints, TetGenResult->pointlist, TetGenResult->numberoftetrahedra * 3, tet_indices.data(),
        Material.YoungModulus, Material.PoissonRatio, Material.Density};

    m2f::CommonArguments args{
        "modalModel",
        true, // freq control activated
        20, // lowest mode freq
        10000, // highest mode freq
        40, // number of synthesized modes (default is 20)
        80, // number of modes to be computed for the finite element analysis (default is 100)
        ExcitableVertexIndices, // specific excitation positions
        int(ExcitableVertexIndices.size()), // number of excitation positions (default is max: -1)
    };
    return m2f::mesh2faust(&volumetric_mesh, args);
}

static constexpr float VertexHoverRadius = 5.f;
static constexpr float VertexHoverRadiusSquared = VertexHoverRadius * VertexHoverRadius;

using namespace ImGui;

void Mesh::UpdateHoveredVertex() {
    const auto &geometry = GetActiveGeometry();

    // Find the hovered vertex, favoring the one nearest to the camera if multiple are hovered.
    HoveredVertexIndex = -1;
    if (IsWindowHovered()) {
        const vec3 camera_position = glm::inverse(Scene.CameraView)[3];
        const auto content_region = GetContentRegionAvail();
        const auto &io = ImGui::GetIO();
        // Transform each vertex position to screen space and check if the mouse is hovering over it.
        const auto &mouse_pos = io.MousePos;
        const auto &content_pos = GetWindowPos() + GetWindowContentRegionMin();
        const auto &view_projection = Scene.CameraProjection * Scene.CameraView;
        float min_vertex_camera_distance = FLT_MAX;
        const auto &vertices = geometry.Vertices;
        for (size_t i = 0; i < vertices.size(); i++) {
            const auto &v = vertices[i];
            const vec4 pos_clip_space = view_projection * vec4{v.x, v.y, v.z, 1.0f};
            const vec4 tmp = (pos_clip_space / pos_clip_space.w) * 0.5f + 0.5f;
            const auto vertex_screen = ImVec2{tmp.x, 1.0f - tmp.y} * content_region + content_pos;
            const float screen_dist_squared = pow(mouse_pos.x - vertex_screen.x, 2) + pow(mouse_pos.y - vertex_screen.y, 2);
            if (screen_dist_squared <= VertexHoverRadiusSquared) {
                const float vertex_camera_distance = glm::distance(camera_position, v);
                if (vertex_camera_distance < min_vertex_camera_distance) {
                    min_vertex_camera_distance = vertex_camera_distance;
                    HoveredVertexIndex = i;
                }
            }
        }
    }

    HoveredVertexArrow.Transforms.clear();
    if (HoveredVertexIndex >= 0 && HoveredVertexIndex < int(geometry.Vertices.size())) {
        // Point the arrow at the hovered vertex.
        float scale_factor = 0.1f * glm::distance(InitialBounds.first, InitialBounds.second);
        mat4 scale = glm::scale(Identity, vec3{scale_factor});
        mat4 translate = glm::translate(Identity, geometry.Vertices[HoveredVertexIndex]);
        mat4 rotate = glm::toMat4(glm::rotation(Up, glm::normalize(geometry.Normals[HoveredVertexIndex])));
        HoveredVertexArrow.Transforms.push_back({translate * rotate * scale});
    }
}

void Mesh::Update() {
    if (ViewMeshType == MeshType_Tetrahedral && !HasTetMesh()) SetViewMeshType(MeshType_Triangular);
    UpdateHoveredVertex();
    UpdateExcitableVertexColors();
}

void Mesh::Render() {
    const auto &geometry = GetActiveGeometry();
    const auto &vertices = geometry.Vertices;

    // Handle mouse interactions.
    const bool mouse_clicked = IsMouseClicked(0), mouse_released = IsMouseReleased(0);
    if (!Scene.ShowGizmo && IsWindowHovered()) {
        if (mouse_clicked) CameraTargetVertexIndex = -1;
        else if (mouse_released) CameraTargetVertexIndex = HoveredVertexIndex;

        // On click, trigger the nearest excitation vertex nearest to the clicked vertex.
        if (mouse_clicked && HoveredVertexIndex >= 0 && !ExcitableVertexIndices.empty() && Audio::FaustState::IsRunning() && !Audio::FaustState::Is2DModel) {
            const auto &hovered_vertex = vertices[HoveredVertexIndex];
            int nearest_excite_vertex_pos = -1; // Position in the excitation vertex indices.
            float min_dist = FLT_MAX;
            for (size_t i = 0; i < ExcitableVertexIndices.size(); i++) {
                const auto &excite_vertex_index = ExcitableVertexIndices[i];
                const auto &excite_vertex = vertices[excite_vertex_index];
                const float dist = glm::distance(excite_vertex, hovered_vertex);
                if (dist < min_dist) {
                    min_dist = dist;
                    nearest_excite_vertex_pos = i;
                }
            }
            if (nearest_excite_vertex_pos >= 0) { // Shouldn't ever be false, but sanity check.
                *Audio::FaustState::ExcitePos = nearest_excite_vertex_pos;
                *Audio::FaustState::ExciteValue = 1.f;
            }
        }
    }

    // When mouse is released, release the excitation trigger
    // (even if mouse was pressed, dragged outside the window, and released).
    if (mouse_released && Audio::FaustState::IsRunning()) *Audio::FaustState::ExciteValue = 0.f;

    // Rotate the camera to the targeted vertex.
    if (CameraTargetVertexIndex >= 0) {
        static const float CameraMovementSpeed = 0.5;

        const vec3 camera_position = glm::inverse(Scene.CameraView)[3];
        const auto &target_vertex = vertices[CameraTargetVertexIndex];
        const vec3 target_direction = glm::normalize(target_vertex - Origin);
        // Calculate the direction from the origin to the camera position
        const vec3 current_direction = glm::normalize(camera_position - Origin);
        // Create quaternions representing the two directions
        const glm::quat current_quat = glm::quatLookAt(current_direction, Up);
        const glm::quat target_quat = glm::quatLookAt(target_direction, Up);
        // Interpolate linearly between the two quaternions along the sphere defined by the camera distance.
        const float lerp_factor = glm::clamp(CameraMovementSpeed / Scene.CameraDistance, 0.0f, 1.0f);
        const glm::quat new_quat = glm::slerp(current_quat, target_quat, lerp_factor);
        const vec3 new_camera_direction = new_quat * glm::vec3(0.0f, 0.0f, -1.0f);
        Scene.CameraView = glm::lookAt(Origin + new_camera_direction * Scene.CameraDistance, Origin, Up);
    }
}

void Mesh::RenderConfig() {
    if (BeginTabBar("MeshConfigTabBar")) {
        if (BeginTabItem("Mesh")) {
            const auto &center = GetMainViewport()->GetCenter();
            SetNextWindowPos(center, ImGuiCond_Appearing, {0.5f, 0.5f});
            SetNextWindowSize(GetMainViewport()->Size / 4);
            if (TetGenerator.Render()) {
                // Tet generation completed.
                UpdateTetMesh();
                SetViewMeshType(MeshType_Tetrahedral); // Automatically switch to tetrahedral view.
            }
            SeparatorText("Create");

            const bool can_generate_tet_mesh = !MeshProfile::ClosePath;
            if (!can_generate_tet_mesh) {
                BeginDisabled();
                TextUnformatted("Disable |MeshProfile|->|ClosePath| to generate tet mesh.\n(Meshes with holes can only be used for axisymmetric simulations.)");
            }
            Checkbox("Quality mode", &QualityTetMesh);
            TetGenerator.RenderLauncher(HasTetMesh() ? "Regenerate tetrahedral mesh" : "Generate tetrahedral mesh");
            if (!can_generate_tet_mesh) EndDisabled();

            SeparatorText("Current mesh");
            Text("File: %s", FilePath.c_str());
            if (HasTetMesh()) {
                Checkbox("Show excitable vertices", &ShowExcitableVertices);
                if (SliderInt("Num. excitable vertices", &NumExcitableVertices, 1, std::min(200, int(TetMesh.Vertices.size())))) {
                    UpdateExcitableVertices();
                }

                Text(
                    "Current tetrahedral mesh:\n\tVertices: %lu\n\tIndices: %lu",
                    TetMesh.Vertices.size(), TetMesh.TriangleIndices.size()
                );
                TextUnformatted("View mesh type");
                Type mesh_type = ViewMeshType;
                bool mesh_type_changed = RadioButton("Triangular", &mesh_type, MeshType_Triangular);
                SameLine();
                mesh_type_changed |= RadioButton("Tetrahedral", &mesh_type, MeshType_Tetrahedral);
                if (mesh_type_changed) SetViewMeshType(mesh_type);
            } else {
                TextUnformatted("No tetrahedral mesh loaded");
            }

            SeparatorText("Transform");
            if (Checkbox("Gizmo##Transform", &Scene.ShowGizmo)) {
                if (Scene.ShowGizmo) {
                    Scene.GizmoTransform = GetTransform();
                    Scene.GizmoCallback = [this](const glm::mat4 &transform) {
                        TriangularMesh.SetTransform(transform);
                        TetMesh.SetTransform(transform);
                        Translation = glm::vec3{transform[3]};
                        Scale = {glm::length(transform[0]), glm::length(transform[1]), glm::length(transform[2])};
                        RotationAngles = glm::eulerAngles(glm::quat_cast(transform));
                    };
                } else {
                    Scene.GizmoCallback = nullptr;
                }
            }
            Scene.RenderGizmoDebug();

            Text("Translate");
            if (SliderFloat("x##Translate", &Translation.x, -1, 1)) ApplyTransform();
            if (SliderFloat("y##Translate", &Translation.y, -1, 1)) ApplyTransform();
            if (SliderFloat("z##Translate", &Translation.z, -1, 1)) ApplyTransform();

            Text("Scale");
            static bool lock_scale = true;
            Checkbox("Lock##Scale", &lock_scale);
            if (lock_scale) {
                if (SliderFloat("Scale##Scale", &Scale.x, 0.1, 10, "%.3f", ImGuiSliderFlags_Logarithmic)) {
                    Scale.y = Scale.z = Scale.x;
                    ApplyTransform();
                }
            } else {
                if (SliderFloat("x##Scale", &Scale.x, 0.1, 10, "%.3f", ImGuiSliderFlags_Logarithmic)) ApplyTransform();
                if (SliderFloat("y##Scale", &Scale.y, 0.1, 10, "%.3f", ImGuiSliderFlags_Logarithmic)) ApplyTransform();
                if (SliderFloat("z##Scale", &Scale.z, 0.1, 10, "%.3f", ImGuiSliderFlags_Logarithmic)) ApplyTransform();
            }

            Text("Rotate");
            if (SliderFloat("x##Rotate", &RotationAngles.x, -180.f, 180.f)) ApplyTransform();
            if (SliderFloat("y##Rotate", &RotationAngles.y, -180.f, 180.f)) ApplyTransform();
            if (SliderFloat("z##Rotate", &RotationAngles.z, -180.f, 180.f)) ApplyTransform();

            SeparatorText("Debug");
            if (HoveredVertexIndex >= 0) {
                const auto &vertex = GetActiveGeometry().Vertices[HoveredVertexIndex];
                Text("Hovered vertex:\n\tIndex: %d\n\tPosition:\n\t\tx: %f\n\t\ty: %f\n\t\tz: %f", HoveredVertexIndex, vertex.x, vertex.y, vertex.z);
            }

            EndTabItem();
        }
        if (BeginTabItem("Mesh profile")) {
            RenderProfileConfig();
            EndTabItem();
        }
        if (BeginTabItem("RealImpact")) {
            if (!RealImpact) {
                if (std::filesystem::exists(FilePath.parent_path() / RealImpact::SampleDataFileName)) {
                    RealImpactLoader.RenderLauncher();
                } else {
                    Text("No RealImpact data found in the same directory as the mesh.");
                }
            } else {
                RealImpact->Render();
            }
            if (RealImpactLoader.Render() && RealImpact) {
                const size_t num_points = RealImpact->NumListenerPoints();
                RealImpactListenerPoints.Transforms.clear();
                RealImpactListenerPoints.Colors.clear();
                for (size_t i = 0; i < num_points; i++) {
                    RealImpactListenerPoints.Transforms.push_back(glm::translate(Identity, RealImpact->ListenerPoint(i)));
                    RealImpactListenerPoints.Colors.push_back({1, 1, 1, 1});
                }
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
