//
// Created by smith on 2026-06-19.
//

#include "Framebuffer.h"
#include <glad/glad.h>
#include <iostream>

Framebuffer::Framebuffer(int width, int height) {
    create(width, height);
}

Framebuffer::~Framebuffer() {
    destroy();
}

// Build the framebuffer and attach a colour texture + a depth renderbuffer.
// A framebuffer on its own is just an empty container; it does nothing until we
// "attach" somewhere for the GPU to write colour and depth.
void Framebuffer::create(int width, int height) {
    w = width;
    h = height;

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // --- COLOUR ATTACHMENT: an ordinary texture -------------------------------
    // This is the bit that makes offscreen rendering useful: the scene's colour
    // lands in a texture we can later sample like any other image.
    glGenTextures(1, &colorTex);
    glBindTexture(GL_TEXTURE_2D, colorTex);
    // Passing nullptr for the data means "allocate the storage but leave it
    // uninitialised" -- the GPU will fill it when we render into this target.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    // LINEAR filtering matters for the upscaler later: when the offscreen image
    // is smaller than the window, the screen quad smoothly interpolates it.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // Clamp so edge pixels don't wrap around when a post effect samples slightly
    // outside [0,1] (e.g. a blur reaching past the border).
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // Wire the texture in as "colour output slot 0" of this framebuffer.
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, colorTex, 0);

    // --- DEPTH ATTACHMENT: a renderbuffer -------------------------------------
    // We still need a depth buffer so the 3D scene depth-tests correctly while
    // rendering into this target. But we never READ the depth back, so a
    // renderbuffer (write-only, can't be sampled) is cheaper than a texture.
    glGenRenderbuffers(1, &depthRbo);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, depthRbo);

    // Sanity check: the framebuffer must have everything it needs or rendering
    // to it silently does nothing.
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "Offscreen framebuffer is not complete!\n";

    glBindFramebuffer(GL_FRAMEBUFFER, 0);   // back to the window
}

void Framebuffer::destroy() {
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &colorTex);
    glDeleteRenderbuffers(1, &depthRbo);
    fbo = colorTex = depthRbo = 0;
}

void Framebuffer::resize(int width, int height) {
    if (width == w && height == h) return;   // nothing changed -> nothing to do
    destroy();
    create(width, height);
}

void Framebuffer::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}

void Framebuffer::unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
