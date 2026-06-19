//
// Created by smith on 2026-06-18.
//

// tiny_obj_loader.h is a "single-header library": all its code lives in the
// header. Defining this macro in EXACTLY ONE .cpp file tells it to compile its
// implementation here. Every other file just gets the declarations.
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include "Mesh.h"
#include <glad/glad.h>
#include <cstddef>      // offsetof
#include <iostream>

std::vector<Vertex> loadOBJ(const std::string& path) {
    // tinyobjloader fills these three containers for us:
    tinyobj::attrib_t attrib;                 // raw pools of positions/normals/uvs
    std::vector<tinyobj::shape_t> shapes;     // each object's list of face indices
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    bool ok = tinyobj::LoadObj(&attrib, &shapes, &materials,
                               &warn, &err, path.c_str());

    if (!warn.empty()) std::cout << "OBJ warning: " << warn << '\n';
    if (!err.empty())  std::cerr << "OBJ error: "   << err  << '\n';
    if (!ok) {
        std::cerr << "Failed to load OBJ: " << path << '\n';
        return {};   // empty list -> nothing draws, but we don't crash
    }

    // In an .obj, positions and normals are stored in separate pools, and each
    // face vertex is a set of indices INTO those pools. We "flatten" that here:
    // for every face vertex we copy out its actual position + normal, producing
    // one plain list the GPU can draw with glDrawArrays.
    std::vector<Vertex> vertices;
    for (const tinyobj::shape_t& shape : shapes) {
        for (const tinyobj::index_t& idx : shape.mesh.indices) {
            Vertex v{};

            // Each position is 3 floats, so the i-th position starts at 3*i.
            v.position = {
                attrib.vertices[3 * idx.vertex_index + 0],
                attrib.vertices[3 * idx.vertex_index + 1],
                attrib.vertices[3 * idx.vertex_index + 2],
            };

            // normal_index is -1 if the file has no normals for this vertex.
            if (idx.normal_index >= 0) {
                v.normal = {
                    attrib.normals[3 * idx.normal_index + 0],
                    attrib.normals[3 * idx.normal_index + 1],
                    attrib.normals[3 * idx.normal_index + 2],
                };
            } else {
                v.normal = {0.0f, 0.0f, 0.0f};
            }

            // texcoord_index is -1 if the file has no UVs. Note UVs are 2 floats
            // each (not 3), so the i-th one starts at 2*i.
            if (idx.texcoord_index >= 0) {
                v.uv = {
                    attrib.texcoords[2 * idx.texcoord_index + 0],
                    attrib.texcoords[2 * idx.texcoord_index + 1],
                };
            } else {
                v.uv = {0.0f, 0.0f};
            }

            vertices.push_back(v);
        }
    }

    std::cout << "Loaded " << path << " (" << vertices.size() << " vertices)\n";
    return vertices;
}

Mesh::Mesh(const std::vector<Vertex>& vertices) {
    vertexCount = static_cast<int>(vertices.size());

    // Create the two GPU objects (same pattern as before, now owned by Mesh).
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    // Copy the whole vertex list to the GPU in one go. Because each Vertex is
    // just two glm::vec3s laid out back-to-back in memory, we can upload the
    // vector's bytes directly.
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                 vertices.data(),
                 GL_STATIC_DRAW);

    // attribute 0 = position : 3 floats, found at the start of each Vertex.
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(0);

    // attribute 1 = normal : 3 floats, found right after the position.
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);

    // attribute 2 = uv : 2 floats (not 3), found after the normal.
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, uv));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);   // unbind so later code can't change this VAO by accident
}

Mesh::~Mesh() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
}

void Mesh::draw() const {
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
}
