#include "pch.h"
#include "GardenTreeRenderer.h"
#include <glew.h>

GardenTreeRenderer::GardenTreeRenderer(
    Framework::MeshProperties* meshRef,
    const char* vsPath,
    const char* fsPath)
    : mesh(meshRef)
{
    shader = new ShaderProgram(vsPath, fsPath);
    shader->initGL();

    glGenBuffers(1, &instanceVBO);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // Base mesh VBO / EBO
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo);

    const GLsizei stride = sizeof(float) * 8; // pos(3), normal(3), uv(2)

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));

    // Instance buffer
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);

    const size_t vec4Size = sizeof(glm::vec4);
    const size_t instanceStride = sizeof(GardenTreeInstance);

    // model matrix -> 3,4,5,6
    for (int i = 0; i < 4; ++i)
    {
        glEnableVertexAttribArray(3 + i);
        glVertexAttribPointer(
            3 + i,
            4,
            GL_FLOAT,
            GL_FALSE,
            (GLsizei)instanceStride,
            (void*)(offsetof(GardenTreeInstance, model) + vec4Size * i)
        );
        glVertexAttribDivisor(3 + i, 1);
    }

    // opacity -> 7
    glEnableVertexAttribArray(7);
    glVertexAttribPointer(
        7,
        1,
        GL_FLOAT,
        GL_FALSE,
        (GLsizei)instanceStride,
        (void*)offsetof(GardenTreeInstance, opacity)
    );
    glVertexAttribDivisor(7, 1);

    // variation -> 8
    glEnableVertexAttribArray(8);
    glVertexAttribPointer(
        8,
        1,
        GL_FLOAT,
        GL_FALSE,
        (GLsizei)instanceStride,
        (void*)offsetof(GardenTreeInstance, variation)
    );
    glVertexAttribDivisor(8, 1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

GardenTreeRenderer::~GardenTreeRenderer()
{
    if (instanceVBO) glDeleteBuffers(1, &instanceVBO);
    if (vao) glDeleteVertexArrays(1, &vao);

    if (shader)
    {
        shader->destroy();
        delete shader;
        shader = nullptr;
    }
}

void GardenTreeRenderer::setInstances(const std::vector<GardenTreeInstance>& instances)
{
    instanceCount = static_cast<int>(instances.size());
    if (instanceCount == 0) return;

    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferData(
        GL_ARRAY_BUFFER,
        instanceCount * sizeof(GardenTreeInstance),
        instances.data(),
        GL_STATIC_DRAW
    );
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GardenTreeRenderer::draw(
    const glm::mat4& view,
    const glm::mat4& proj,
    const glm::mat4& model,
    GLuint textureID)
{
    if (instanceCount == 0) return;

    shader->use();
    shader->setMat4("uView", view);
    shader->setMat4("uProj", proj);
    shader->setMat4("uModel", model);
    shader->setInt("uTexture", 0);
    shader->setVec3("uAmbientColor", glm::vec3(1.0f));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindVertexArray(vao);

    glDrawElementsInstanced(
        GL_TRIANGLES,
        static_cast<GLsizei>(mesh->indices.size()),
        GL_UNSIGNED_INT,
        0,
        instanceCount
    );

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);
}