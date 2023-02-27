#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <stdio.h>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"
#include "imgui_internal.h"
#include "implot.h"
#include <SDL.h>
#include <SDL_opengl.h>

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
static float fovy = 90.0f;
static float zNear = 0.1f, zFar = 99.0f;
static int amountinit = 5, amount;
static vec3 eyeinit(0.0, 0.0, 5.0), upinit(0.0, 1.0, 0.0), center(0.0, 0.0, 0.0);
static vec3 eye, up;

// Lighting details
const int numLights = 5;

// Variables to set uniform params for lighting fragment shader
static GLuint lightcol, lightpos, ambientcol, diffusecol, specularcol, emissioncol, shininesscol;

// Callback and reshape globals
static int keyboard_mode = 0, mouse_mode = 1, render_mode = 0;

static GLuint vertexshader, fragmentshader, shaderprogram;

static float sx, sy;
static float tx, ty;

float ambient[4] = {0.05, 0.05, 0.05, 1};
float diffusion[4] = {0.2, 0.2, 0.2, 1};
float specular[4] = {0.5, 0.5, 0.5, 1};
float light_positions[20] = {0.0f};
float light_colors[20] = {0.0f};
float shininess = 10;
bool custom_color = false;

void initialise_shader_and_mesh() {
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
    mesh.Init();
    mesh.Load(fs::path("res") / "obj" / "bunny.obj");
}

void display(float &ambient_slider, float &diffuse_slider, float &specular_slider, float &shininess_slider, bool custom_color, float &light_position, float &light_color) {
    static mat4 modelview;
    modelview = glm::lookAt(eye, center, up);
    glUniformMatrix4fv(modelviewPos, 1, GL_FALSE, &modelview[0][0]);

    glUniform4fv(lightpos, numLights, &light_position);
    glUniform4fv(lightcol, numLights, &light_color);

    // Transformations for objects, involving translation and scaling
    mat4 sc = Transform::scale(sx, sy, 1.0);
    mat4 tr = Transform::translate(tx, ty, 0.0);
    modelview = tr * sc * modelview;

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

    glUniformMatrix4fv(modelviewPos, 1, GL_FALSE, &(modelview * glm::scale(mat4(1.0f), vec3(2, 2, 2)))[0][0]);
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
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows
    // io.ConfigViewportsNoAutoMerge = true;
    // io.ConfigViewportsNoTaskBarIcon = true;
    io.IniFilename = nullptr; // Disable ImGui's .ini file saving

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle &style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

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

    State s{}; // Main application state

    // Initialise all variable initial values
    initialise_shader_and_mesh();

    static GlCanvas gl_canvas;

    eye = (eyeinit);
    up = (upinit);
    amount = amountinit;
    sx = sy = 1.0;
    tx = ty = 0.0;

    glEnable(GL_DEPTH_TEST);

    light_positions[1] = 6.5f;
    light_colors[0] = 1.0f;
    light_colors[1] = 1.0f;
    light_colors[2] = 1.0f;

    render_mode = 3;

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
            if (event.type == SDL_EVENT_QUIT)
                done = true;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        auto dockspace_id = ImGui::DockSpaceOverViewport(nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
        if (ImGui::GetFrameCount() == 1) {
            auto demo_node_id = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.3f, nullptr, &dockspace_id);
            ImGui::DockBuilderDockWindow(s.Windows.ImGuiDemo.Name, demo_node_id);
            ImGui::DockBuilderDockWindow(s.Windows.ImPlotDemo.Name, demo_node_id);
            ImGui::DockBuilderDockWindow(s.Windows.Main.Name, dockspace_id);
            auto mesh_node_id = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.75f, nullptr, &dockspace_id);
            auto mesh_controls_node_id = ImGui::DockBuilderSplitNode(mesh_node_id, ImGuiDir_Left, 0.3f, nullptr, &mesh_node_id);
            ImGui::DockBuilderDockWindow(s.Windows.MeshControls.Name, mesh_controls_node_id);
            ImGui::DockBuilderDockWindow(s.Windows.Mesh.Name, mesh_node_id);
        }

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Windows")) {
                ImGui::MenuItem(s.Windows.Main.Name, nullptr, &s.Windows.Main.Visible);
                ImGui::MenuItem(s.Windows.MeshControls.Name, nullptr, &s.Windows.MeshControls.Visible);
                ImGui::MenuItem(s.Windows.Mesh.Name, nullptr, &s.Windows.Mesh.Visible);
                ImGui::MenuItem(s.Windows.ImGuiDemo.Name, nullptr, &s.Windows.ImGuiDemo.Visible);
                ImGui::MenuItem(s.Windows.ImPlotDemo.Name, nullptr, &s.Windows.ImPlotDemo.Visible);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
        if (s.Windows.ImGuiDemo.Visible)
            // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
            ImGui::ShowDemoWindow(&s.Windows.ImGuiDemo.Visible);
        if (s.Windows.ImPlotDemo.Visible)
            ImPlot::ShowDemoWindow(&s.Windows.ImPlotDemo.Visible);
        if (s.Windows.Main.Visible) {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin(s.Windows.Main.Name, &s.Windows.Main.Visible);

            ImGui::Text("This is some useful text."); // Display some text (you can use a format strings too)

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f); // Edit 1 float using a slider from 0.0f to 1.0f

            if (ImGui::Button("Button")) // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();
        }

        if (s.Windows.MeshControls.Visible) {
            ImGui::Begin(s.Windows.MeshControls.Name, &s.Windows.MeshControls.Visible);

            ImGui::Separator();
            ImGui::TextColored({0.0f, 1.0f, 1.0f, 1.0f}, "Render Mode");
            ImGui::Separator();
            // ImGui::Text("Primitive object "); ImGui::SameLine();
            ImGui::RadioButton("Smooth", &render_mode, 0);
            ImGui::SameLine();
            ImGui::RadioButton("Lines", &render_mode, 1);
            ImGui::SameLine();
            ImGui::RadioButton("Point Cloud", &render_mode, 2);
            ImGui::SameLine();
            ImGui::RadioButton("Mesh", &render_mode, 3);

            ImGui::Separator();
            ImGui::TextColored({0.0f, 1.0f, 1.0f, 1.0f}, "Object Properties");
            ImGui::Separator();
            ImGui::TextWrapped(
                "These set of boxes and sliders change the way the object reacts with light giving us the impression of its material."
                "The initial sliders assume the object stays white, and 'Custom Colors' allows changing the associated colors"
            );
            ImGui::Separator();

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
            ImGui::Text(" ");

            ImGui::Separator();
            ImGui::TextColored({0.0f, 1.0f, 1.0f, 1.0f}, "Input Devices");
            ImGui::Separator();
            ImGui::TextWrapped("Decide which input device associates to which transformation."
                               "By default the scroll wheel is associated with scaling");
            ImGui::Separator();

            ImGui::Text("Use mouse to ");
            ImGui::SameLine();
            ImGui::RadioButton("translate", &mouse_mode, 0);
            ImGui::SameLine();
            ImGui::RadioButton("rotate", &mouse_mode, 1);

            ImGui::Text("Using keyboard to");
            ImGui::SameLine();
            ImGui::RadioButton("translate ", &keyboard_mode, 0);
            ImGui::SameLine();
            ImGui::RadioButton("rotate ", &keyboard_mode, 1);

            keyboard_mode = !(mouse_mode);

            ImGui::Text(" ");

            ImGui::Separator();
            ImGui::TextColored({0.0f, 1.0f, 1.0f, 1.0f}, "Camera");
            ImGui::Separator();
            ImGui::SliderFloat("Field of view", &fovy, 0.0f, 180.0f);
            ImGui::SliderFloat("Frustum near plane", &zNear, 0.0f, 15.0f);
            ImGui::SliderFloat("Frustum far plane", &zFar, 0.0f, 150.0f);
            ImGui::Text(" ");
            ImGui::SliderFloat3("Camera position ", &eye[0], -10.0f, 10.0f);

            ImGui::Separator();
            ImGui::TextColored({0.0f, 1.0f, 1.0f, 1.0f}, "Lighting");
            ImGui::Separator();
            ImGui::TextWrapped("Most lights are switched off by default, and the below"
                               "sliders can play with the light positions and color intensities");

            for (int i = 0; i < numLights; i++) {
                const string light_name = string("Light ") + std::to_string(i + 1);
                ImGui::Separator();
                ImGui::Text("Light");
                ImGui::Separator();
                ImGui::SliderFloat3((light_name + " positions").c_str(), &light_positions[4 * i], -25.0f, 25.0f);
                ImGui::SliderFloat3((light_name + " colors").c_str(), &light_colors[4 * i], 0.0f, 1.0f);

                light_positions[(4 * i) + 3] = 1.0f;
                light_colors[(4 * i) + 3] = 1.0f;
            }

            ImGui::End();
        }

        if (s.Windows.Mesh.Visible) {
            ImGui::Begin(s.Windows.Mesh.Name, &s.Windows.Mesh.Visible);
            if (gl_canvas.SetupRender()) {
                static mat4 projection;
                projection = glm::perspective(glm::radians(fovy), gl_canvas.Width / gl_canvas.Height, zNear, zFar);
                glUniformMatrix4fv(projectionPos, 1, GL_FALSE, &projection[0][0]);
                display(*ambient, *diffusion, *specular, shininess, custom_color, *light_positions, *light_colors);

                gl_canvas.Render();
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

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
