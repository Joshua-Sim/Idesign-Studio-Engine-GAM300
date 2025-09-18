#include "pch.h"
#include "Mesh.h"

// Destructor
Mesh::~Mesh() {
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
}

Mesh Mesh::CreateUnitCubeData() {
    Mesh m;

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

    m.vertexCount = 36;
    m.vertices.assign(V, V + sizeof(V) / sizeof(float));
    return m;
}

void Mesh::initGL() {
    if (vao != 0) return; // already uploaded

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    // layout(location=0) position, layout(location=1) normal
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

    glBindVertexArray(0);

    // we can free CPU data if you don't need it anymore
    vertices.clear();
    vertices.shrink_to_fit();
}

void Mesh::draw() const {
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    glBindVertexArray(0);
}

// move semantics
Mesh::Mesh(Mesh&& other) noexcept {
    vao = other.vao; vbo = other.vbo; vertexCount = other.vertexCount;
    vertices = std::move(other.vertices);
    other.vao = 0; other.vbo = 0; other.vertexCount = 0;
}
Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        if (vbo) glDeleteBuffers(1, &vbo);
        if (vao) glDeleteVertexArrays(1, &vao);
        vao = other.vao; vbo = other.vbo; vertexCount = other.vertexCount;
        vertices = std::move(other.vertices);
        other.vao = 0; other.vbo = 0; other.vertexCount = 0;
    }
    return *this;
}