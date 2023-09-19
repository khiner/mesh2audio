#include "GlCanvas.h"

#include "GL/glew.h"
#include <stdexcept>

GlCanvas::~GlCanvas() {
    Destroy();
}

void GlCanvas::SetupRender(float width, float height, float r, float g, float b, float a) {
    if (width != Width || height != Height) {
        // Render image to twice the dimensions, for better quality.
        Width = width * 2;
        Height = height * 2;
        Destroy();

        glGenFramebuffers(1, &FrameBufferId);
        glBindFramebuffer(GL_FRAMEBUFFER, FrameBufferId);

        glGenTextures(1, &TextureId);
        glBindTexture(GL_TEXTURE_2D, TextureId);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, Width, Height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        glGenRenderbuffers(1, &DepthRenderBufferId);
        glBindRenderbuffer(GL_RENDERBUFFER, DepthRenderBufferId);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, Width, Height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, DepthRenderBufferId);

        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, TextureId, 0);

        GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, DrawBuffers); // `1` is the size of DrawBuffers
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            throw std::runtime_error("Error: Framebuffer is not complete.\n");
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, FrameBufferId);
    glViewport(0, 0, Width, Height);

    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

unsigned int GlCanvas::Render() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return TextureId;
}

// Private

void GlCanvas::Destroy() {
    glDeleteRenderbuffers(1, &DepthRenderBufferId);
    glDeleteTextures(1, &TextureId);
    glDeleteFramebuffers(1, &FrameBufferId);
}
