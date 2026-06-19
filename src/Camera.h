//
// Created by smith on 2026-06-18.
//

#ifndef RENDERER_CAMERA_H
#define RENDERER_CAMERA_H

#endif //RENDERER_CAMERA_H

#pragma once
#include "glm/fwd.hpp"
#include "glm/glm.hpp"

class Camera {
public:
    void move_forward(float DeltaTime);
    void move_backward(float DeltaTime);
    void move_left(float DeltaTime);
    void move_right(float DeltaTime);

    glm::mat4 get_lookat();

    float speed;
    glm::vec3 up;
    glm::vec3 center;
    glm::vec3 eye;
    Camera();
};
