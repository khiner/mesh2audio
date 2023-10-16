#pragma once

using uint = unsigned int;

// Render an OpenGL frame buffer to a texture.
// Uses MSAA if `SubsamplesPerPixel` > 1.
struct GLCanvas {
    ~GLCanvas();

    // RGBA background color.
    void PrepareRender(uint width, uint height, float r, float g, float b, float a);
    uint Render(); // Returns `TextureId` after binding the frame buffer.

private:
    uint Width = 0, Height = 0;
    uint SubsamplesPerPixel = 4;
    uint FrameBufferId, TextureId, DepthRenderBufferId, ResolveBufferId, ResolveTextureId;

    void Destroy();
};
