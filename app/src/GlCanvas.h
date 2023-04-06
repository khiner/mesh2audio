#pragma once

// Render an OpenGL frame buffer to a texture.
struct GlCanvas {
    ~GlCanvas();

    // RGBA background color.
    void SetupRender(float width, float height, float r, float g, float b, float a);
    unsigned int Render(); // Returns `TextureId` after binding the frame buffer.


private:
    float Width = 0, Height = 0;
    unsigned int FrameBufferId, TextureId, DepthRenderBufferId;

    void Destroy();
};
