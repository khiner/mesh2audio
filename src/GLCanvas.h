#pragma once

using uint = unsigned int;

#include <glm/vec4.hpp>

// Render an OpenGL frame buffer to a texture.
// Uses MSAA if `SubsamplesPerPixel` > 1.
struct GLCanvas {
    ~GLCanvas();

    // RGBA background color.
    void PrepareRender(uint width, uint height, const glm::vec4 &bg_color);
    uint Render(); // Returns `TextureId` after binding the frame buffer.

private:
    uint Width = 0, Height = 0;
    uint SubsamplesPerPixel = 4;
    uint FrameBufferId, TextureId, DepthRenderBufferId, ResolveBufferId, ResolveTextureId;

    void Destroy();
};
