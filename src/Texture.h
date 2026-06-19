//
// Created by smith on 2026-06-18.
//

#ifndef RENDERER_TEXTURE_H
#define RENDERER_TEXTURE_H

#include <string>

// A Texture owns one image uploaded to the GPU. Same bind-and-configure pattern
// as Mesh's buffers, just a new object type (GL_TEXTURE_2D).
class Texture {
public:
    // Load an image file from disk and upload it to the GPU.
    explicit Texture(const std::string& path);

    // Free the GPU texture when destroyed.
    ~Texture();

    // Make this texture active on a "texture unit" (slot) so the shader's
    // sampler can read it. unit 0 is the default.
    void bind(unsigned int unit = 0) const;

    // Owns a raw GPU handle -> forbid copying to avoid a double-free.
    Texture(const Texture&)            = delete;
    Texture& operator=(const Texture&) = delete;

private:
    unsigned int id = 0;   // the GPU texture handle
};

#endif //RENDERER_TEXTURE_H
