//
// Created by smith on 2026-06-18.
//

// stb_image.h is a single-header library, exactly like tiny_obj_loader.h.
// This macro must be defined in EXACTLY ONE .cpp in the whole project (here),
// or the linker sees two copies of its code and fails with "multiple definition".
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "Texture.h"
#include <glad/glad.h>
#include <iostream>

Texture::Texture(const std::string& path) {
    // .obj UVs put (0,0) at the bottom-left, but image files store rows top-down.
    // Flipping on load lines them up so the texture isn't upside down.
    stbi_set_flip_vertically_on_load(true);

    int width, height, channels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
    if (!data) {
        std::cerr << "Failed to load texture: " << path << '\n';
        return;   // id stays 0 -> nothing samples, but we don't crash
    }

    // Pick the matching OpenGL format from however many channels stb found.
    GLenum format = GL_RGB;
    if      (channels == 1) format = GL_RED;
    else if (channels == 4) format = GL_RGBA;

    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);

    // Wrapping: what to do for UVs outside 0..1. REPEAT tiles the image.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    // Filtering: how to smooth when the image is scaled. Mipmaps when shrinking,
    // linear blend when enlarging.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Upload the pixel bytes to the GPU, then build the smaller mipmap levels.
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0,
                 format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);   // the GPU has its own copy now; free the CPU one
    std::cout << "Loaded texture " << path << " ("
              << width << "x" << height << ", " << channels << " channels)\n";
}

Texture::~Texture() {
    glDeleteTextures(1, &id);
}

void Texture::bind(unsigned int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);   // choose the slot
    glBindTexture(GL_TEXTURE_2D, id);      // put this texture in it
}
