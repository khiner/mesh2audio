#pragma once

// Render an OpenGL frame buffer to a texture, for drawing to an ImGui window.
struct GlCanvas {
    bool SetupRender();
    void Render();
    void Destroy();

    unsigned int FrameBufferId, TextureId, DepthRenderBufferId;
    float Width = 0, Height = 0;
};
