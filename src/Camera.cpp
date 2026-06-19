//
// Created by smith on 2026-06-18.
//

#include "Camera.h"

#include "glm/fwd.hpp"
#include "glm/ext/matrix_transform.hpp"
    Camera::Camera()
        : eye(glm::vec3(0.0f, 0.0f, 3.0f))
        , front(glm::vec3(0.0f, 0.0f, 0.0f))
        , up(glm::vec3(0.0f, 1.0f, 0.0f))
        , speed(1.0f) {

    }

    void Camera::set_direction(float yaw, float pitch) {
        glm::vec3 f;
        f.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        f.y = sin(glm::radians(pitch));
        f.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(f);
    }



void Camera::move_forward(float dt)  { eye += front * (speed * dt); }
void Camera::move_backward(float dt) { eye -= front * (speed * dt); }
void Camera::move_left(float dt)     { eye -= glm::normalize(glm::cross(front, up)) * (speed * dt); }
void Camera::move_right(float dt)    { eye += glm::normalize(glm::cross(front, up)) * (speed * dt); }

    glm::mat4 Camera::get_lookat() {
        return glm::lookAt(eye, eye+front, up);
    }

