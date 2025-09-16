#pragma once
#include <glew.h>
#include <vector>
#include <glm.hpp>

class Terrain {
public:
    Terrain(int size, float scale);

    ~Terrain();

    void draw() const;

    float getHeightAt(float x, float z) const;


private:
    int size;       // number of vertices along one axis
    float scale;    // spacing between vertices
    std::vector<float> heights;

    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLsizei indexCount = 0;
};
