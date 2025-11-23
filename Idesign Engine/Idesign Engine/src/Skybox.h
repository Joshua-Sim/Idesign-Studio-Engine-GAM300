#pragma once
#include <glew.h>
#include <vector>
#include <string>
#include <glm.hpp>
#include "ShaderProgram.h"

class Skybox {
public:
    Skybox();
    ~Skybox();

    // Now accepts TWO lists of faces
    void init(const std::vector<std::string>& dayFaces,
        const std::vector<std::string>& nightFaces);

    void draw(const glm::mat4& view, const glm::mat4& proj);

    // New helper to control the cycle
    void setBlendFactor(float factor) { m_blendFactor = factor; }
    float getBlendFactor() const { return m_blendFactor; }

private:
    unsigned int cubeVAO = 0;
    unsigned int cubeVBO = 0;

    // Store two IDs now
    unsigned int texDay = 0;
    unsigned int texNight = 0;

    float m_blendFactor = 0.0f; // Start at Day (0.0)

    ShaderProgram* shader = nullptr;

    unsigned int loadCubemap(const std::vector<std::string>& faces);
};