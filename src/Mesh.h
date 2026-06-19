//
// Created by smith on 2026-06-18.
//

#ifndef RENDERER_MESH_H
#define RENDERER_MESH_H

#include <glm/glm.hpp>
#include <string>
#include <vector>

// One vertex of a mesh: where it is, which way the surface faces, and where it
// samples the texture. The normal drives shading; the uv maps the image on.
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

// Read a .obj file off disk and turn it into a flat list of vertices,
// ready to hand straight to the GPU (3 vertices per triangle, in order).
std::vector<Vertex> loadOBJ(const std::string& path);

// A Mesh owns the GPU buffers for one model and knows how to draw it.
class Mesh {
public:
    // Upload the given vertices to the GPU.
    explicit Mesh(const std::vector<Vertex>& vertices);

    // Free the GPU buffers when the Mesh is destroyed.
    ~Mesh();

    // Draw the mesh with whatever shader program is currently active.
    void draw() const;

    // A Mesh owns raw GPU handles, so copying it would lead to a double-free.
    // We forbid copying; the Mesh is created once and lives where it is made.
    Mesh(const Mesh&)            = delete;
    Mesh& operator=(const Mesh&) = delete;

private:
    unsigned int VAO = 0;   // remembers how to read the vertex data
    unsigned int VBO = 0;   // the actual vertex bytes on the GPU
    int vertexCount  = 0;   // how many vertices to draw
};

#endif //RENDERER_MESH_H
