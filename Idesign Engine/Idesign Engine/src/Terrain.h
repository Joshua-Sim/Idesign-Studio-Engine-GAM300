#pragma once
#include <glew.h>
#include <vector>
#include <glm.hpp>

class Terrain {
public:
    Terrain(int size, float scale);

    // init GL resources AFTER OpenGL context exists
    void initGL();

    void draw() const;

    float getHeightAt(float x, float z) const;

private:
    int size;
    float scale;
    std::vector<float> heights;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    GLuint vao = 0, vbo = 0, ebo = 0;
    GLsizei indexCount = 0;
};