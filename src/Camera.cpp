//
// Created by smith on 2026-06-18.
//

#include "Camera.h"

#include "glm/fwd.hpp"
#include "glm/ext/matrix_transform.hpp"
    Camera::Camera()
        : eye(glm::vec3(0.0f, 0.0f, 3.0f))
        , center(glm::vec3(0.0f, 0.0f, 0.0f))
        , up(glm::vec3(0.0f, 1.0f, 0.0f))
        , speed(1.0f) {

    }

    void Camera::move_forward(float DeltaTime) {
        eye.z -= DeltaTime*speed;
        center.z -= DeltaTime*speed;
    }
    void Camera::move_backward(float DeltaTime) {
        eye.z += DeltaTime*speed;
        center.z += DeltaTime*speed;
    }
    void Camera::move_left(float DeltaTime) {
        eye.x -= DeltaTime*speed;
        center.x -= DeltaTime*speed;
    }
    void Camera::move_right(float DeltaTime) {
        eye.x += DeltaTime*speed;
        center.x += DeltaTime*speed;
    }

    glm::mat4 Camera::get_lookat() {
        return glm::lookAt(eye, center, up);
    }

