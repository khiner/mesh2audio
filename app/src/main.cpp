#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <stdio.h>

#include "imgui.h"

#include "ImGuizmo.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"
#include "implot.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include <nfd.h>

#include "GlCanvas.h"
#include "Mesh.h"
#include "Shader.h"
#include "State.h"
#include "Transform.h"

// This example can also compile and run with Emscripten! See 'Makefile.emscripten' for details.
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

using std::string;

static gl::Mesh mesh;
static GLuint projectionPos, modelviewPos;

static const int numLights = 5;
// Variables to set uniform params for lighting fragment shader
static GLuint lightcol, lightpos, ambientcol, diffusecol, specularcol, emissioncol, shininesscol;

static int render_mode = 3;
static GLuint vertexshader, fragmentshader, shaderprogram;

static float ambient[4] = {0.05, 0.05, 0.05, 1};
static float diffusion[4] = {0.2, 0.2, 0.2, 1};
static float specular[4] = {0.5, 0.5, 0.5, 1};
static float light_positions[20] = {0.0f};
static float light_colors[20] = {0.0f};
static float shininess = 10;
static bool custom_color = false;

static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
static const mat4 identityMatrix(1.f);
static mat4 objectMatrix(1.f), cameraView(1.f), cameraProjection;
static float camDistance = 8.f, fov = 27.f;
static float orthoViewWidth = 10.f;
static bool showGrid = false, isPerspective = true;

static bool showGizmo = false;
static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::LOCAL);
static float snap[3] = {1.f, 1.f, 1.f};
static float bounds[] = {-0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f};
static float boundsSnap[] = {0.1f, 0.1f, 0.1f};
static bool useSnap = false, boundSizing = false, boundSizingSnap = false;

void InitializeShaderAndMesh() {
    // Initialize shaders
    vertexshader = Shader::InitShader(GL_VERTEX_SHADER, fs::path("res") / "shaders" / "vertex.glsl");
    fragmentshader = Shader::InitShader(GL_FRAGMENT_SHADER, fs::path("res") / "shaders" / "fragment.glsl");
    shaderprogram = Shader::InitProgram(vertexshader, fragmentshader);

    // Get uniform locations
    lightpos = glGetUniformLocation(shaderprogram, "light_posn");
    lightcol = glGetUniformLocation(shaderprogram, "light_col");
    ambientcol = glGetUniformLocation(shaderprogram, "ambient");
    diffusecol = glGetUniformLocation(shaderprogram, "diffuse");
    specularcol = glGetUniformLocation(shaderprogram, "specular");
    emissioncol = glGetUniformLocation(shaderprogram, "emission");
    shininesscol = glGetUniformLocation(shaderprogram, "shininess");
    projectionPos = glGetUniformLocation(shaderprogram, "projection");
    modelviewPos = glGetUniformLocation(shaderprogram, "modelview");

    // Initialize global mesh
    mesh.Load(fs::path("res") / "obj" / "car.obj");
}

void Display(float &ambient_slider, float &diffuse_slider, float &specular_slider, float &shininess_slider, bool custom_color, float &light_position, float &light_color) {
    glUniform4fv(lightpos, numLights, &light_position);
    glUniform4fv(lightcol, numLights, &light_color);

    if (!custom_color) {
        *(&ambient_slider + 1) = ambient_slider;
        *(&ambient_slider + 2) = ambient_slider;

        *(&diffuse_slider + 1) = diffuse_slider;
        *(&diffuse_slider + 2) = diffuse_slider;

        *(&specular_slider + 1) = specular_slider;
        *(&specular_slider + 2) = specular_slider;
    }

    glUniform4fv(ambientcol, 1, &ambient_slider);
    glUniform4fv(diffusecol, 1, &diffuse_slider);
    glUniform4fv(specularcol, 1, &specular_slider);
    glUniform1f(shininesscol, shininess_slider);

    glBindVertexArray(mesh.vertex_array);
    if (render_mode == 0) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDrawElements(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, 0);
    }
    if (render_mode == 1) {
        glLineWidth(1);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDrawElements(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, 0);
    }
    if (render_mode == 2) {
        glPointSize(2.5);
        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
        glDrawElements(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, 0);
    }
    if (render_mode == 3) {
        const static GLfloat black[4] = {0, 0, 0, 0}, white[4] = {1, 1, 1, 1};

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDrawElements(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, 0);
        glUniform4fv(diffusecol, 1, black);
        glUniform4fv(specularcol, 1, white);

        glPointSize(2.5);
        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
        glDrawElements(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, 0);

        glLineWidth(2.5);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDrawElements(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, 0);
    }
    glBindVertexArray(0);
}

int main(int, char **) {
    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMEPAD) != 0) {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // Decide GL+GLSL versions
#if defined(__APPLE__)
    // GL 3.2 Core + GLSL 150
    const char *glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    // GL 3.0 + GLSL 130
    const char *glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    // Enable native IME.
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_Window *window = SDL_CreateWindow("mesh2audio", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    if (glewInit() != GLEW_OK) {
        std::cout << "Error initializing `glew`.\n";
        return -1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows
    // io.ConfigViewportsNoAutoMerge = true;
    // io.ConfigViewportsNoTaskBarIcon = true;
    io.IniFilename = nullptr; // Disable ImGui's .ini file saving

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle &style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    State s{}; // Main application state

    // Initialize file dialog & audio device.
    NFD_Init();
    s.Audio.Init();

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See Makefile.emscripten for details.
    // io.Fonts->AddFontDefault();
    // io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    // ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    // IM_ASSERT(font != NULL);

    // Initialise all variable initial values
    InitializeShaderAndMesh();

    static GlCanvas gl_canvas;

    glEnable(GL_DEPTH_TEST);

    light_positions[1] = 6.5f;
    light_colors[0] = light_colors[1] = light_colors[2] = 1.0f;

    // Main loop
    bool done = false;
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = NULL;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!done)
#endif
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT || (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window)))
                done = true;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        auto dockspace_id = ImGui::DockSpaceOverViewport(nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
        if (ImGui::GetFrameCount() == 1) {
            auto audio_node_id = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.3f, nullptr, &dockspace_id);
            auto faust_code_node_id = ImGui::DockBuilderSplitNode(audio_node_id, ImGuiDir_Right, 0.5f, nullptr, &audio_node_id);
            ImGui::DockBuilderDockWindow(s.Windows.AudioDevice.Name, audio_node_id);
            ImGui::DockBuilderDockWindow(s.Windows.FaustCode.Name, faust_code_node_id);
            auto demo_node_id = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.3f, nullptr, &dockspace_id);
            ImGui::DockBuilderDockWindow(s.Windows.ImGuiDemo.Name, demo_node_id);
            ImGui::DockBuilderDockWindow(s.Windows.ImPlotDemo.Name, demo_node_id);
            auto mesh_node_id = dockspace_id;
            auto mesh_controls_node_id = ImGui::DockBuilderSplitNode(mesh_node_id, ImGuiDir_Left, 0.3f, nullptr, &mesh_node_id);
            ImGui::DockBuilderDockWindow(s.Windows.MeshControls.Name, mesh_controls_node_id);
            ImGui::DockBuilderDockWindow(s.Windows.Mesh.Name, mesh_node_id);
        }

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Load mesh", nullptr)) {
                    nfdchar_t *obj_file_path;
                    nfdfilteritem_t filter[] = {{"Mesh object", "obj"}};
                    nfdresult_t result = NFD_OpenDialog(&obj_file_path, filter, 1, "res/obj/");
                    if (result == NFD_OKAY) {
                        // Load object file.
                        mesh.Load(obj_file_path);
                        NFD_FreePath(obj_file_path);
                    } else if (result != NFD_CANCEL) {
                        std::cerr << "Error: " << NFD_GetError() << '\n';
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Windows")) {
                ImGui::MenuItem(s.Windows.AudioDevice.Name, nullptr, &s.Windows.AudioDevice.Visible);
                ImGui::MenuItem(s.Windows.FaustCode.Name, nullptr, &s.Windows.FaustCode.Visible);
                ImGui::MenuItem(s.Windows.MeshControls.Name, nullptr, &s.Windows.MeshControls.Visible);
                ImGui::MenuItem(s.Windows.Mesh.Name, nullptr, &s.Windows.Mesh.Visible);
                ImGui::MenuItem(s.Windows.ImGuiDemo.Name, nullptr, &s.Windows.ImGuiDemo.Visible);
                ImGui::MenuItem(s.Windows.ImPlotDemo.Name, nullptr, &s.Windows.ImPlotDemo.Visible);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        if (s.Windows.ImGuiDemo.Visible) ImGui::ShowDemoWindow(&s.Windows.ImGuiDemo.Visible);
        if (s.Windows.ImPlotDemo.Visible) ImPlot::ShowDemoWindow(&s.Windows.ImPlotDemo.Visible);

        if (s.Windows.MeshControls.Visible) {
            ImGui::Begin(s.Windows.MeshControls.Name, &s.Windows.MeshControls.Visible);

            ImGui::SeparatorText("Camera");
            ImGui::Checkbox("Show grid", &showGrid);
            if (ImGui::RadioButton("Perspective", isPerspective)) isPerspective = true;
            ImGui::SameLine();
            if (ImGui::RadioButton("Orthographic", !isPerspective)) isPerspective = false;
            if (isPerspective) ImGui::SliderFloat("Fov", &fov, 20.f, 110.f);
            else ImGui::SliderFloat("Ortho width", &orthoViewWidth, 1, 20);

            static bool viewDirty = true;
            viewDirty |= ImGui::SliderFloat("Distance", &camDistance, 1.f, 10.f);
            if (viewDirty) {
                static float camYAngle = 165.f / 180.f * M_PI;
                static float camXAngle = 32.f / 180.f * M_PI;
                const vec3 eye = {cosf(camYAngle) * cosf(camXAngle) * camDistance, sinf(camXAngle) * camDistance, sinf(camYAngle) * cosf(camXAngle) * camDistance};
                static const vec3 center = {0.f, 0.f, 0.f};
                static const vec3 up = {0.f, 1.f, 0.f};
                cameraView = glm::lookAt(eye, center, up);
            }

            ImGui::SeparatorText("Gizmo");
            ImGui::Checkbox("Show gizmo", &showGizmo);
            if (showGizmo) {
                ImGui::Text("X: %f Y: %f", io.MousePos.x, io.MousePos.y);
                if (ImGuizmo::IsUsing()) {
                    ImGui::Text("Using gizmo");
                } else {
                    ImGui::Text(ImGuizmo::IsOver() ? "Over gizmo" : "");
                    ImGui::SameLine();
                    ImGui::Text(ImGuizmo::IsOver(ImGuizmo::TRANSLATE) ? "Over translate gizmo" : "");
                    ImGui::SameLine();
                    ImGui::Text(ImGuizmo::IsOver(ImGuizmo::ROTATE) ? "Over rotate gizmo" : "");
                    ImGui::SameLine();
                    ImGui::Text(ImGuizmo::IsOver(ImGuizmo::SCALE) ? "Over scale gizmo" : "");
                }

                if (ImGui::IsKeyPressed(ImGuiKey_T)) mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
                if (ImGui::IsKeyPressed(ImGuiKey_E)) mCurrentGizmoOperation = ImGuizmo::ROTATE;
                if (ImGui::IsKeyPressed(ImGuiKey_R)) mCurrentGizmoOperation = ImGuizmo::SCALE;
                if (ImGui::RadioButton("Translate", mCurrentGizmoOperation == ImGuizmo::TRANSLATE)) mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
                ImGui::SameLine();
                if (ImGui::RadioButton("Rotate", mCurrentGizmoOperation == ImGuizmo::ROTATE)) mCurrentGizmoOperation = ImGuizmo::ROTATE;
                ImGui::SameLine();
                if (ImGui::RadioButton("Scale", mCurrentGizmoOperation == ImGuizmo::SCALE)) mCurrentGizmoOperation = ImGuizmo::SCALE;
                if (ImGui::RadioButton("Universal", mCurrentGizmoOperation == ImGuizmo::UNIVERSAL)) mCurrentGizmoOperation = ImGuizmo::UNIVERSAL;

                if (mCurrentGizmoOperation != ImGuizmo::SCALE) {
                    if (ImGui::RadioButton("Local", mCurrentGizmoMode == ImGuizmo::LOCAL)) mCurrentGizmoMode = ImGuizmo::LOCAL;
                    ImGui::SameLine();
                    if (ImGui::RadioButton("World", mCurrentGizmoMode == ImGuizmo::WORLD)) mCurrentGizmoMode = ImGuizmo::WORLD;
                }
                if (ImGui::IsKeyPressed(ImGuiKey_S)) useSnap = !useSnap;
                ImGui::Checkbox("##UseSnap", &useSnap);
                ImGui::SameLine();

                switch (mCurrentGizmoOperation) {
                    case ImGuizmo::TRANSLATE:
                        ImGui::InputFloat3("Snap", &snap[0]);
                        break;
                    case ImGuizmo::ROTATE:
                        ImGui::InputFloat("Angle Snap", &snap[0]);
                        break;
                    case ImGuizmo::SCALE:
                        ImGui::InputFloat("Scale Snap", &snap[0]);
                        break;
                }
                ImGui::Checkbox("Bound Sizing", &boundSizing);
                if (boundSizing) {
                    ImGui::PushID(3);
                    ImGui::Checkbox("##BoundSizing", &boundSizingSnap);
                    ImGui::SameLine();
                    ImGui::InputFloat3("Snap", boundsSnap);
                    ImGui::PopID();
                }
            }

            // Mesh controls. TODO tabs for camera/guizmo/mesh?
            ImGui::SeparatorText("Render Mode");
            ImGui::RadioButton("Smooth", &render_mode, 0);
            ImGui::SameLine();
            ImGui::RadioButton("Lines", &render_mode, 1);
            ImGui::SameLine();
            ImGui::RadioButton("Point Cloud", &render_mode, 2);
            ImGui::SameLine();
            ImGui::RadioButton("Mesh", &render_mode, 3);

            ImGui::SeparatorText("Colors");
            ImGui::Checkbox("Custom Colors", &custom_color);
            if (custom_color) {
                ImGui::SliderFloat3("Ambient R, G, B", &ambient[0], 0.0f, 1.0f);
                ImGui::SliderFloat3("Diffusion R, G, B", &diffusion[0], 0.0f, 1.0f);
                ImGui::SliderFloat3("Specular R, G, B", &specular[0], 0.0f, 1.0f);
                ImGui::SliderFloat("Shininess", &shininess, 0.0f, 150.0f);
            } else {
                ImGui::SliderFloat("Ambient", &ambient[0], 0.0f, 1.0f);
                ImGui::SliderFloat("Diffusion", &diffusion[0], 0.0f, 1.0f);
                ImGui::SliderFloat("Specular", &specular[0], 0.0f, 1.0f);
                ImGui::SliderFloat("Shininess", &shininess, 0.0f, 150.0f);
            }

            ImGui::SeparatorText("Lighting");
            for (int i = 0; i < numLights; i++) {
                ImGui::Separator();
                ImGui::PushID(i);
                ImGui::Text("Light %d", i + 1);
                ImGui::SliderFloat3("Positions", &light_positions[4 * i], -25.0f, 25.0f);
                ImGui::ColorEdit3("Color", &light_colors[4 * i]);
                ImGui::PopID();

                light_positions[(4 * i) + 3] = 1.0f;
                light_colors[(4 * i) + 3] = 1.0f;
            }

            ImGui::End();
        }

        s.Audio.Update();
        if (s.Windows.AudioDevice.Visible) {
            ImGui::Begin(s.Windows.AudioDevice.Name, &s.Windows.AudioDevice.Visible);
            s.Audio.Device.Render();
            ImGui::End();
        }
        if (s.Windows.FaustCode.Visible) {
            ImGui::Begin(s.Windows.FaustCode.Name, &s.Windows.FaustCode.Visible);
            ImGui::InputTextMultiline("##Faust", &s.Audio.Faust.Code, ImGui::GetContentRegionAvail());
            ImGui::End();
        }
        if (s.Windows.Mesh.Visible) {
            ImGui::Begin(s.Windows.Mesh.Name, &s.Windows.Mesh.Visible);

            const auto content_region = ImGui::GetContentRegionAvail();
            if (isPerspective) {
                cameraProjection = glm::perspective(glm::radians(fov * 2), content_region.x / content_region.y, 0.1f, 100.f);
            } else {
                const float viewHeight = orthoViewWidth * content_region.y / content_region.x;
                cameraProjection = glm::ortho(-orthoViewWidth, orthoViewWidth, -viewHeight, viewHeight, 1000.f, -1000.f);
            }
            ImGuizmo::SetOrthographic(!isPerspective);

            if (gl_canvas.SetupRender(content_region.x, content_region.y)) {
                glUniformMatrix4fv(projectionPos, 1, GL_FALSE, &cameraProjection[0][0]);
                const mat4 modelView = cameraView * objectMatrix;
                glUniformMatrix4fv(modelviewPos, 1, GL_FALSE, &modelView[0][0]);
                Display(*ambient, *diffusion, *specular, shininess, custom_color, *light_positions, *light_colors);

                gl_canvas.Render();
            }
            if (showGizmo || showGrid) {
                ImGuizmo::BeginFrame();
                ImGuizmo::SetDrawlist();
                ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, ImGui::GetWindowWidth(), ImGui::GetWindowHeight());
            }
            if (showGrid) {
                ImGuizmo::DrawGrid(&cameraView[0][0], &cameraProjection[0][0], &identityMatrix[0][0], 100.f);
            }
            if (showGizmo) {
                const float viewManipulateRight = io.DisplaySize.x, viewManipulateTop = 0;
                // This is how you would draw a test cube:
                // ImGuizmo::DrawCubes(&cameraView[0][0], &cameraProjection[0][0], &objectMatrix[0][0], 1);
                ImGuizmo::Manipulate(
                    &cameraView[0][0], &cameraProjection[0][0], mCurrentGizmoOperation, mCurrentGizmoMode, &objectMatrix[0][0], nullptr,
                    useSnap ? &snap[0] : nullptr, boundSizing ? bounds : nullptr, boundSizingSnap ? boundsSnap : nullptr
                );
                const static ImU32 bg_color = 0x10101010;
                ImGuizmo::ViewManipulate(&cameraView[0][0], camDistance, {viewManipulateRight - 128, viewManipulateTop}, {128, 128}, bg_color);
            }
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call SDL_GL_MakeCurrent(window, gl_context) directly)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            SDL_Window *backup_current_window = SDL_GL_GetCurrentWindow();
            SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
        }

        SDL_GL_SwapWindow(window);
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    mesh.Destroy();
    gl_canvas.Destroy();

    s.Audio.Destroy();
    NFD_Quit();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
