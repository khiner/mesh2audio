#include "GLCanvas.h"
#include "GL/glew.h"
#include <stdexcept>

const GLenum ColorFormat = GL_RGB;
const GLenum DepthFormat = GL_DEPTH_COMPONENT;
const GLenum DataType = GL_UNSIGNED_BYTE;

GLCanvas::~GLCanvas() {
    Destroy();
}

static void CheckFramebufferStatus() {
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        throw std::runtime_error("Framebuffer is not complete.\n");
    }
}

static void SetTextureParameters() {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

static void CreateTexture(unsigned int &id, GLenum target, uint width, uint height, int samples) {
    glGenTextures(1, &id);
    glBindTexture(target, id);
    if (samples > 1) {
        glTexImage2DMultisample(target, samples, ColorFormat, width, height, GL_TRUE);
    } else {
        glTexImage2D(target, 0, ColorFormat, width, height, 0, ColorFormat, DataType, nullptr);
    }
}

static void CreateRenderbuffer(unsigned int &id, GLenum format, uint samples, uint width, uint height) {
    glGenRenderbuffers(1, &id);
    glBindRenderbuffer(GL_RENDERBUFFER, id);
    if (samples > 1) {
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, format, width, height);
    } else {
        glRenderbufferStorage(GL_RENDERBUFFER, format, width, height);
    }
}

void GLCanvas::PrepareRender(uint width, uint height, const glm::vec4 &bg_color) {
    if (width != Width || height != Height) {
        Destroy();
        Width = width;
        Height = height;

        glGenFramebuffers(1, &FrameBufferId);
        glBindFramebuffer(GL_FRAMEBUFFER, FrameBufferId);

        GLenum texture_target = SubsamplesPerPixel > 1 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
        CreateTexture(TextureId, texture_target, Width, Height, SubsamplesPerPixel);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture_target, TextureId, 0);

        CreateRenderbuffer(DepthRenderBufferId, DepthFormat, SubsamplesPerPixel, Width, Height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, DepthRenderBufferId);

        CheckFramebufferStatus();

        glGenFramebuffers(1, &ResolveBufferId);
        glBindFramebuffer(GL_FRAMEBUFFER, ResolveBufferId);

        CreateTexture(ResolveTextureId, GL_TEXTURE_2D, Width, Height, 1);
        SetTextureParameters();
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ResolveTextureId, 0);

        CheckFramebufferStatus();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, FrameBufferId);
    glViewport(0, 0, Width, Height);
    glClearColor(bg_color.r, bg_color.g, bg_color.b, bg_color.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

uint GLCanvas::Render() {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, FrameBufferId);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ResolveBufferId);
    glBlitFramebuffer(0, 0, Width, Height, 0, 0, Width, Height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return ResolveTextureId;
}

void GLCanvas::Destroy() {
    glDeleteRenderbuffers(1, &DepthRenderBufferId);
    glDeleteTextures(1, &TextureId);
    glDeleteFramebuffers(1, &FrameBufferId);
    glDeleteFramebuffers(1, &ResolveBufferId);
    glDeleteTextures(1, &ResolveTextureId);
}
