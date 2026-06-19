//
// Created by smith on 2026-06-19.
//

#ifndef RENDERER_TRANSFORM_H
#define RENDERER_TRANSFORM_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// A Transform is "where an object is in the world", stored as three intuitive
// pieces a human can reason about: where it sits, how it is turned, and how big
// it is. The renderer doesn't speak position/rotation/scale though -- it speaks
// 4x4 matrices. matrix() converts our friendly numbers into the single uModel
// matrix the vertex shader multiplies every vertex by.
struct Transform {
    glm::vec3 position = glm::vec3(0.0f);          // world-space location
    glm::vec3 rotation = glm::vec3(0.0f);          // Euler angles in DEGREES (x,y,z)
    glm::vec3 scale    = glm::vec3(1.0f);          // per-axis size multiplier

    // Combine the three parts into one model matrix. The multiplication order
    // matters: read right-to-left, a vertex is first SCALED, then ROTATED, then
    // TRANSLATED. (Translate-last is what keeps an object rotating about its own
    // centre instead of being flung around the world origin.)
    glm::mat4 matrix() const {
        glm::mat4 m(1.0f);
        m = glm::translate(m, position);
        // Rotate about each axis in turn. glm::rotate wants radians, so convert.
        m = glm::rotate(m, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        m = glm::rotate(m, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        m = glm::rotate(m, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        m = glm::scale(m, scale);
        return m;
    }
};

#endif //RENDERER_TRANSFORM_H
