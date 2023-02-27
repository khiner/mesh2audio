#pragma once

struct State {
    struct Window {
        const char *Name{""};
        bool Visible{true};
    };

    struct WindowsState {
        Window MeshControls{"Mesh Controls"};
        Window Mesh{"Mesh"};
        Window ImGuiDemo{"Dear ImGui Demo"};
        Window ImPlotDemo{"ImPlot Demo"};
    };

    WindowsState Windows{};
};
