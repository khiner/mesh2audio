#include "GlCanvas.h"

#include "GL/glew.h"
#include "imgui.h"
#include <stdexcept>

bool GlCanvas::SetupRender() {
    const auto avail_size = ImGui::GetContentRegionAvail();
    if (avail_size.x == 0 || avail_size.y == 0) return false;

    if (avail_size.x != Width || avail_size.y != Height) {
        Width = avail_size.x;
        Height = avail_size.y;
        Destroy();

        glGenFramebuffers(1, &FrameBufferId);
        glBindFramebuffer(GL_FRAMEBUFFER, FrameBufferId);

        glGenTextures(1, &TextureId);
        glBindTexture(GL_TEXTURE_2D, TextureId);
        // Render image to twice the dimensions, for better quality.
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, Width * 2, Height * 2, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenRenderbuffers(1, &DepthRenderBufferId);
        glBindRenderbuffer(GL_RENDERBUFFER, DepthRenderBufferId);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, Width * 2, Height * 2);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, DepthRenderBufferId);

        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, TextureId, 0);

        GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, DrawBuffers); // "1" is the size of DrawBuffers
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            throw std::runtime_error("Error: Framebuffer is not complete.\n");
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, FrameBufferId);
    glViewport(0, 0, Width * 2, Height * 2); // Rendered image is twice the dimensions, for better quality.

    const auto bg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
    glClearColor(bg.x, bg.y, bg.z, bg.w);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    return true;
}

void GlCanvas::Render() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    ImGui::Image((void *)(intptr_t)TextureId, {Width, Height}, {0, 1}, {1, 0});
}

void GlCanvas::Destroy() {
    glDeleteRenderbuffers(1, &DepthRenderBufferId);
    glDeleteTextures(1, &TextureId);
    glDeleteFramebuffers(1, &FrameBufferId);
}
