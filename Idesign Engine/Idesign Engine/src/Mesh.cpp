#include "pch.h"
#include "Mesh.h"

// Interleaved: position (3), normal (3)
static void generateCube(GLuint& vao, GLuint& vbo, GLsizei& count) {
    // 36 vertices (6 faces * 2 tris * 3 verts), each with pos+normal
    // Unit cube centered at origin, normals per face
    const float V[] = {
        // +X
        0.5f,-0.5f,-0.5f,  1,0,0,   0.5f, 0.5f,-0.5f,  1,0,0,   0.5f, 0.5f, 0.5f,  1,0,0,
        0.5f,-0.5f,-0.5f,  1,0,0,   0.5f, 0.5f, 0.5f,  1,0,0,   0.5f,-0.5f, 0.5f,  1,0,0,
        // -X
       -0.5f,-0.5f, 0.5f, -1,0,0,  -0.5f, 0.5f, 0.5f, -1,0,0,  -0.5f, 0.5f,-0.5f, -1,0,0,
       -0.5f,-0.5f, 0.5f, -1,0,0,  -0.5f, 0.5f,-0.5f, -1,0,0,  -0.5f,-0.5f,-0.5f, -1,0,0,
       // +Y
      -0.5f, 0.5f,-0.5f,  0,1,0,  -0.5f, 0.5f, 0.5f,  0,1,0,   0.5f, 0.5f, 0.5f,  0,1,0,
      -0.5f, 0.5f,-0.5f,  0,1,0,   0.5f, 0.5f, 0.5f,  0,1,0,   0.5f, 0.5f,-0.5f, 0,1,0,
      // -Y
     -0.5f,-0.5f, 0.5f,  0,-1,0, -0.5f,-0.5f,-0.5f, 0,-1,0,   0.5f,-0.5f,-0.5f, 0,-1,0,
     -0.5f,-0.5f, 0.5f,  0,-1,0,  0.5f,-0.5f,-0.5f, 0,-1,0,   0.5f,-0.5f, 0.5f, 0,-1,0,
     // +Z
    -0.5f,-0.5f, 0.5f,  0,0,1,   0.5f,-0.5f, 0.5f,  0,0,1,   0.5f, 0.5f, 0.5f,  0,0,1,
    -0.5f,-0.5f, 0.5f,  0,0,1,   0.5f, 0.5f, 0.5f,  0,0,1,  -0.5f, 0.5f, 0.5f,  0,0,1,
    // -Z
    0.5f,-0.5f,-0.5f,  0,0,-1, -0.5f,-0.5f,-0.5f, 0,0,-1,  -0.5f, 0.5f,-0.5f, 0,0,-1,
    0.5f,-0.5f,-0.5f,  0,0,-1, -0.5f, 0.5f,-0.5f, 0,0,-1,   0.5f, 0.5f,-0.5f, 0,0,-1,
    };
    count = 36;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(V), V, GL_STATIC_DRAW);

    // layout(location=0) position, layout(location=1) normal
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

    glBindVertexArray(0);
}

Mesh Mesh::CreateUnitCube() {
    GLuint vao = 0, vbo = 0; GLsizei count = 0;
    generateCube(vao, vbo, count);
    return Mesh(vao, vbo, count);
}

Mesh::~Mesh() {
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
}

void Mesh::draw() const {
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    glBindVertexArray(0);
}

Mesh::Mesh(Mesh&& other) noexcept {
    vao = other.vao; vbo = other.vbo; vertexCount = other.vertexCount;
    other.vao = 0; other.vbo = 0; other.vertexCount = 0;
}
Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        if (vbo) glDeleteBuffers(1, &vbo);
        if (vao) glDeleteVertexArrays(1, &vao);
        vao = other.vao; vbo = other.vbo; vertexCount = other.vertexCount;
        other.vao = 0; other.vbo = 0; other.vertexCount = 0;
    }
    return *this;
}
