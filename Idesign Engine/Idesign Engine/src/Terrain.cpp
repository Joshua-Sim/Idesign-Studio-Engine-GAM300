#include "pch.h"
#include "Terrain.h"
#include <glm.hpp>
#include <cmath>

Terrain::Terrain(int size, float scale)
    : size(size), scale(scale), heights(size* size)
{
    // All flat height = 0
    for (int z = 0; z < size; ++z)
    {
        for (int x = 0; x < size; ++x)
        {
            heights[z * size + x] = 0.0f; // flat
        }
    }

    auto getIndex = [&](int x, int z) { return z * size + x; };

    for (int z = 0; z < size; ++z)
    {
        for (int x = 0; x < size; ++x)
        {
            float worldX = ((float)x - size * 0.5f) * scale;
            float worldZ = ((float)z - size * 0.5f) * scale;

            float h = 0.0f;

            glm::vec3 normal(0.0f, 1.0f, 0.0f);

            vertices.push_back(worldX);
            vertices.push_back(h);
            // TERRAIN CENTERED: worldZ negative direction might flip uv; keep consistent
            vertices.push_back(worldZ);

            // normal
            vertices.push_back(normal.x);
            vertices.push_back(normal.y);
            vertices.push_back(normal.z);

            // ADD UVs (simple planar mapping)
            float u = (float)x / (float)(size - 1);
            float v = (float)z / (float)(size - 1);
            vertices.push_back(u);
            vertices.push_back(v);

        }
    }

    for (int z = 0; z < size - 1; ++z)
    {
        for (int x = 0; x < size - 1; ++x)
        {
            unsigned int i0 = getIndex(x, z);
            unsigned int i1 = getIndex(x + 1, z);
            unsigned int i2 = getIndex(x, z + 1);
            unsigned int i3 = getIndex(x + 1, z + 1);

            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);

            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }

    indexCount = (GLsizei)indices.size();
}

void Terrain::initGL() {
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    size_t stride = 8 * sizeof(float);

    // position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);

    // normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));

    // uv
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));

    glBindVertexArray(0);
}

void Terrain::draw() const {
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

float Terrain::getHeightAt(float x, float z) const {
    // naive nearest-neighbor sampling (you can extend to bilinear)

    float offset = size * 0.5f;
    int gx = (int)((x / scale) + offset);
    int gz = (int)((z / scale) + offset);

    // Boundary checks
    if (gx < 0 || gz < 0 || gx >= size || gz >= size) return 0.0f;

    return heights[gz * size + gx];
}
