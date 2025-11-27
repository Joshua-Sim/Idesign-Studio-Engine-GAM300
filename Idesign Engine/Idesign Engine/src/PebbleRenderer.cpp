#include "pch.h"
#include "PebbleRenderer.h"
#include <glew.h>

PebbleRenderer::PebbleRenderer(Framework::MeshProperties* meshRef)
    : mesh(meshRef)
{
    shader = new ShaderProgram(
        "Assets/shaders/instanced_pebble_vert.glsl",
        "Assets/shaders/instanced_pebble_frag.glsl"
    );
    shader->initGL();

    // instance buffer
    glGenBuffers(1, &instanceVBO);

    // Create new VAO for instanced mesh
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // --- Bind the mesh's VBO/EBO ---
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);   // << your OBJ mesh VBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo);

    const GLsizei stride = sizeof(float) * 8; // pos(3) normal(3) uv(2)

    // base mesh attributes
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));

    // --- Instance matrix ---
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);

    for (int i = 0; i < 4; i++)
    {
        glEnableVertexAttribArray(3 + i);
        glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE,
            sizeof(glm::mat4),
            (void*)(sizeof(glm::vec4) * i));

        glVertexAttribDivisor(3 + i, 1);
    }

    glBindVertexArray(0);
}

PebbleRenderer::~PebbleRenderer()
{
    if (instanceVBO) glDeleteBuffers(1, &instanceVBO);
    if (vao) glDeleteVertexArrays(1, &vao);
    delete shader;
}

void PebbleRenderer::setInstances(const std::vector<glm::mat4>& transforms)
{
    instanceCount = transforms.size();
    if (instanceCount == 0) return;

    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferData(GL_ARRAY_BUFFER,
        instanceCount * sizeof(glm::mat4),
        transforms.data(),
        GL_STATIC_DRAW);
}

void PebbleRenderer::draw(const glm::mat4& view,
    const glm::mat4& proj,
    const glm::mat4& model,
    GLuint textureID)
{
    if (instanceCount == 0) return;

    shader->use();
    shader->setMat4("uView", view);
    shader->setMat4("uProj", proj);
    shader->setMat4("uModel", model);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    shader->setInt("uTexture", 0);

    glBindVertexArray(vao);

    glDrawElementsInstanced(
        GL_TRIANGLES,
        mesh->indexCount,   // << from MeshLoader
        GL_UNSIGNED_INT,
        0,
        instanceCount
    );

    glBindVertexArray(0);
}
