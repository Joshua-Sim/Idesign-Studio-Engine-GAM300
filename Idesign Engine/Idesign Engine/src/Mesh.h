#pragma once
#include <glew.h>

class Mesh {
public:
    Mesh() = default;
    ~Mesh();

    // Factory: CPU-only data creation
    static Mesh CreateUnitCubeData();

    // Upload to GPU (must be called after context exists)
    void initGL();

    void draw() const;

    // move semantics
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

private:
    std::vector<float> vertices;   // CPU-side vertex data (pos+normal interleaved)
    GLuint vao = 0, vbo = 0;
    GLsizei vertexCount = 0;
};
