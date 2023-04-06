#pragma once

struct Window {
    const char *Name{""};
    bool Visible{true};
};

struct WindowsState {
    Window AudioDevice{"Audio Device"};
    Window FaustCode{"Faust Code"};
    Window MeshControls{"Mesh Controls"};
    Window Mesh{"Mesh"};
    Window MeshProfile{"Mesh Profile"};
    // By default, these demo windows are docked, but not visible.
    Window ImGuiDemo{"Dear ImGui Demo", false};
    Window ImPlotDemo{"ImPlot Demo", false};
};
