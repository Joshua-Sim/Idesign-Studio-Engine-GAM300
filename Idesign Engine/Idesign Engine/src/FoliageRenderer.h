#pragma once
#include <vector>
#include <glm.hpp>
#include "Mesh.h"
#include "ShaderProgram.h"

class FoliageRenderer {
public:
    FoliageRenderer(Mesh* meshRef);
    ~FoliageRenderer();

    // Setup the instances (call this once or when trees move)
    void setInstances(const std::vector<glm::mat4>& transforms);

    // Update the draw function to accept the texture ID
    void draw(const glm::mat4& view, const glm::mat4& proj, unsigned int textureID);

private:
    Mesh* mesh;
    ShaderProgram* shader;

    unsigned int instanceVBO = 0;
    unsigned int vao = 0; // We need a custom VAO for this
    int instanceCount = 0;
};
