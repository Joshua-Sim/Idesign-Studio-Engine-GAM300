#pragma once
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <vector>
#include "Terrain.h"


class Camera {
public:
    enum class Movement { Forward, Backward, Left, Right, Up, Down };

    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;

    float yaw;   // degrees
    float pitch; // degrees
    float speed;
    float sensitivity;
    float zoom;  // fov in degrees

    Camera(glm::vec3 startPos = glm::vec3(0, 0, 3))
        : position(startPos), worldUp(0, 1, 0), yaw(-90.f), pitch(0.f),
        speed(4.5f), sensitivity(0.12f), zoom(60.f) {
        front = glm::vec3(0, 0, -1);
        updateVectors();
    }

    glm::mat4 getViewMatrix() const {
        return glm::lookAt(position, position + front, up);
    }

    float getHeightAt(float x, float z, const std::vector<std::vector<float>>& heightmap, float gridSize) {
        int xi = (int)floor(x / gridSize);
        int zi = (int)floor(z / gridSize);

        // clamp to valid range
        xi = std::max(0, std::min(xi, (int)heightmap.size() - 2));
        zi = std::max(0, std::min(zi, (int)heightmap[0].size() - 2));

        float tx = (x / gridSize) - xi;
        float tz = (z / gridSize) - zi;

        // bilinear interpolate
        float h00 = heightmap[xi][zi];
        float h10 = heightmap[xi + 1][zi];
        float h01 = heightmap[xi][zi + 1];
        float h11 = heightmap[xi + 1][zi + 1];

        float h0 = h00 * (1 - tx) + h10 * tx;
        float h1 = h01 * (1 - tx) + h11 * tx;
        return h0 * (1 - tz) + h1 * tz;
    }


    void processKeyboard(Movement m, float dt, bool groundMode, const Terrain& terrain) {
        float v = speed * dt;

        if (groundMode) {
            glm::vec3 forwardXZ = glm::normalize(glm::vec3(front.x, 0.0f, front.z));
            glm::vec3 rightXZ = glm::normalize(glm::vec3(right.x, 0.0f, right.z));

            if (m == Movement::Forward)  position += forwardXZ * v;
            if (m == Movement::Backward) position -= forwardXZ * v;
            if (m == Movement::Left)     position -= rightXZ * v;
            if (m == Movement::Right)    position += rightXZ * v;

            // ask terrain for ground height
            float groundY = terrain.getHeightAt(position.x, position.z);
            position.y = groundY + 1.8f;
        }
        else {
            // free-fly version
            if (m == Movement::Forward)  position += front * v;
            if (m == Movement::Backward) position -= front * v;
            if (m == Movement::Left)     position -= right * v;
            if (m == Movement::Right)    position += right * v;
            if (m == Movement::Up)       position += worldUp * v;
            if (m == Movement::Down)     position -= worldUp * v;
        }
    }




    void processMouseMovement(float xoffset, float yoffset) {
        xoffset *= sensitivity;
        yoffset *= sensitivity;
        yaw += xoffset;
        pitch += yoffset;
        if (pitch > 89.f) pitch = 89.f;
        if (pitch < -89.f) pitch = -89.f;
        updateVectors();
    }

private:
    void updateVectors() {
        glm::vec3 f;
        f.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        f.y = sin(glm::radians(pitch));
        f.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(f);
        right = glm::normalize(glm::cross(front, worldUp));
        up = glm::normalize(glm::cross(right, front));
    }
};
