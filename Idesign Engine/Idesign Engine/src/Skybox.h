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

    // Loads the 6 textures and sets up buffers
    // Order of faces: Right, Left, Top, Bottom, Front, Back
    void init(const std::vector<std::string>& faces);

    // Draw call needs view and projection matrices
    void draw(const glm::mat4& view, const glm::mat4& proj);

private:
    unsigned int cubeVAO = 0;
    unsigned int cubeVBO = 0;
    unsigned int cubemapTexture = 0;
    
    ShaderProgram* shader = nullptr;

    // Helper to load the raw texture data
    unsigned int loadCubemap(const std::vector<std::string>& faces);
};