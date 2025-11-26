#pragma once
#include <glew.h>
#include <glm.hpp>
class Mesh {
public:
    Mesh() = default;
    ~Mesh();

    // Factory: CPU-only data creation
    static Mesh CreateUnitCubeData();

    static Mesh CreateUnitPlaneXZData();                    // 1x1, y=0, normal +Y
    static Mesh CreateUnitSphereData(int stacks = 16, int slices = 32);
    static Mesh CreateUnitCylinderData(int segments);   // height=1 along Y, r=0.5, with caps
    static Mesh CreateUnitConeData(int slices = 32);
    static Mesh CreateBillboardQuad();

    // Upload to GPU (must be called after context exists)
    void initGL();

    void draw() const;

    // move semantics
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    void cleanupGL();
    static Mesh CreateUnitTriangleData();

    const float* GetRawVertexData() const;
    size_t GetRawDataSize() const;
    unsigned int GetVertexCount() const;

    GLuint getVBO() const { return vbo; }
    int getVertexCount() const { return vertexCount; }

private:
    std::vector<float> vertices;   // CPU-side vertex data (pos+normal interleaved)
    GLuint vao = 0, vbo = 0;
    GLsizei vertexCount = 0;

};
