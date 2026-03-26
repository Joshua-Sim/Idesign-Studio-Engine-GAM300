#pragma once
#include <glm.hpp>
#include <vector>
#include "ShaderProgram.h"
#include "MeshProperties.h"

struct GardenTreeInstance
{
    glm::mat4 model;
    float opacity;
    float variation;
    float padding[2];
};

class GardenTreeRenderer
{
public:
    GardenTreeRenderer(
        Framework::MeshProperties* meshRef,
        const char* vertexShaderPath = "Assets/shaders/GardenTree_vert.glsl",
        const char* fragShaderPath = "Assets/shaders/GardenTree_frag.glsl");
    ~GardenTreeRenderer();

    void setInstances(const std::vector<GardenTreeInstance>& instances);

    void draw(
        const glm::mat4& view,
        const glm::mat4& proj,
        const glm::mat4& model,
        GLuint textureID);

private:
    Framework::MeshProperties* mesh = nullptr;

    GLuint vao = 0;
    GLuint instanceVBO = 0;
    int instanceCount = 0;

    ShaderProgram* shader = nullptr;
};