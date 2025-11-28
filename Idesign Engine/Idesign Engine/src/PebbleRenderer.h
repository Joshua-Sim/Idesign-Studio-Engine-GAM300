#pragma once
#include <glm.hpp>
#include <vector>
#include "ShaderProgram.h"
#include "MeshProperties.h"   // << IMPORTANT

class PebbleRenderer
{
public:
    PebbleRenderer(Framework::MeshProperties* mesh);
    ~PebbleRenderer();

    void setInstances(const std::vector<glm::mat4>& transforms);

    void draw(const glm::mat4& view,
        const glm::mat4& proj,
        const glm::mat4& model,
        GLuint textureID);

private:
    Framework::MeshProperties* mesh;

    GLuint vao = 0;
    GLuint instanceVBO = 0;
    int instanceCount = 0;

    ShaderProgram* shader;
};
