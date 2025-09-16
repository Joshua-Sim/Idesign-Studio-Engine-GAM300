#pragma once
#include <glew.h>

class Mesh {
public:
    Mesh() = default;
    ~Mesh();

    static Mesh CreateUnitCube();

    void draw() const;

    // allow construction from VAO/VBO
    Mesh(GLuint vao, GLuint vbo, GLsizei count)
        : vao(vao), vbo(vbo), vertexCount(count) {
    }

    // disable copy
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    // enable move
    Mesh(Mesh&&) noexcept;
    Mesh& operator=(Mesh&&) noexcept;

private:
    GLuint vao = 0;
    GLuint vbo = 0;
    GLsizei vertexCount = 0;
};
#pragma once
