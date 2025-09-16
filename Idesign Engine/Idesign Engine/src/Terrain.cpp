#include "pch.h"
#include "Terrain.h"
#include <cmath>

Terrain::Terrain(int size, float scale)
    : size(size), scale(scale), heights(size* size)
{
    // generate heights
    for (int z = 0; z < size; ++z) {
        for (int x = 0; x < size; ++x) {
            float worldX = (float)x * scale;
            float worldZ = (float)z * scale;
            float h = sinf(worldX * 0.2f) * cosf(worldZ * 0.2f) * 2.0f; // hills
            heights[z * size + x] = h;
        }
    }

    // build vertex data: pos + normal
    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    auto getIndex = [&](int x, int z) { return z * size + x; };

    for (int z = 0; z < size; ++z) {
        for (int x = 0; x < size; ++x) {
            float worldX = (float)x * scale;
            float worldZ = (float)z * scale;
            float h = heights[getIndex(x, z)];

            // simple normal: sample neighbors
            float hl = (x > 0) ? heights[getIndex(x - 1, z)] : h;
            float hr = (x < size - 1) ? heights[getIndex(x + 1, z)] : h;
            float hd = (z > 0) ? heights[getIndex(x, z - 1)] : h;
            float hu = (z < size - 1) ? heights[getIndex(x, z + 1)] : h;

            glm::vec3 normal = glm::normalize(glm::vec3(hl - hr, 2.0f, hd - hu));

            vertices.push_back(worldX);
            vertices.push_back(h);
            vertices.push_back(worldZ);
            vertices.push_back(normal.x);
            vertices.push_back(normal.y);
            vertices.push_back(normal.z);
        }
    }

    for (int z = 0; z < size - 1; ++z) {
        for (int x = 0; x < size - 1; ++x) {
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

    // OpenGL buffers
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

    glBindVertexArray(0);
}

Terrain::~Terrain() {
    if (ebo) glDeleteBuffers(1, &ebo);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
}

void Terrain::draw() const {
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

float Terrain::getHeightAt(float x, float z) const {
    int xi = (int)floor(x / scale);
    int zi = (int)floor(z / scale);

    if (xi < 0 || zi < 0 || xi >= size - 1 || zi >= size - 1) return 0.0f;

    // bilinear interpolate
    float tx = (x / scale) - xi;
    float tz = (z / scale) - zi;

    auto getH = [&](int x, int z) { return heights[z * size + x]; };

    float h00 = getH(xi, zi);
    float h10 = getH(xi + 1, zi);
    float h01 = getH(xi, zi + 1);
    float h11 = getH(xi + 1, zi + 1);

    float h0 = h00 * (1 - tx) + h10 * tx;
    float h1 = h01 * (1 - tx) + h11 * tx;
    return h0 * (1 - tz) + h1 * tz;
}
