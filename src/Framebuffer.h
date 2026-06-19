//
// Created by smith on 2026-06-19.
//

#ifndef RENDERER_FRAMEBUFFER_H
#define RENDERER_FRAMEBUFFER_H

// A Framebuffer is an OFFSCREEN render target. Normally the GPU draws into the
// window (the "default framebuffer", id 0). Here we make our OWN target whose
// colour ends up in a texture we control. That lets us:
//   1) render the whole scene into a texture, then
//   2) draw a single full-screen quad that samples that texture through a
//      "post-processing" shader (vignette, grayscale, blur, upscaling, ...).
//
// It owns three GPU objects, so -- just like Mesh and Texture -- it forbids
// copying to avoid a double-free.
class Framebuffer {
public:
    // Create the framebuffer and its attachments at the given pixel size.
    Framebuffer(int width, int height);

    // Free all three GPU objects when destroyed.
    ~Framebuffer();

    // Make this the active DRAW target. Everything drawn after this call lands
    // in our colour texture instead of the window.
    void bind() const;

    // Go back to drawing to the window (the default framebuffer, id 0).
    static void unbind();

    // Throw away the attachments and rebuild them at a new size. Call this when
    // the window is resized so the offscreen image keeps matching the window.
    void resize(int width, int height);

    // The texture the scene was rendered into. The post-processing pass samples
    // this in its fragment shader.
    unsigned int colorTexture() const { return colorTex; }

    int width()  const { return w; }
    int height() const { return h; }

    Framebuffer(const Framebuffer&)            = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

private:
    void create(int width, int height);
    void destroy();

    unsigned int fbo      = 0;   // the framebuffer object (the render target)
    unsigned int colorTex = 0;   // colour attachment: a texture we can sample
    unsigned int depthRbo = 0;   // depth attachment: a write-only renderbuffer
    int w = 0, h = 0;            // current size, so resize() can no-op if equal
};

#endif //RENDERER_FRAMEBUFFER_H
