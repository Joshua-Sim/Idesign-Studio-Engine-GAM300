#include "pch.h"
#include "Mesh.h"
#include <array>
#include <cmath>

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

Mesh Mesh::CreateUnitCylinderData(int slices) {
    if (slices < 3) slices = 3;

    Mesh m;
    constexpr float PI = 3.14159265358979323846f;
    const float R = 0.5f;
    const float y0 = -0.5f, y1 = 0.5f;

    auto push = [&](float x, float y, float z, float nx, float ny, float nz) {
        m.vertices.insert(m.vertices.end(), { x,y,z,nx,ny,nz });
        };

    // Sides
    for (int i = 0; i < slices; ++i) {
        float a0 = (i * 2.f * PI) / slices;
        float a1 = ((i + 1) * 2.f * PI) / slices;
        float c0 = std::cos(a0), s0 = std::sin(a0);
        float c1 = std::cos(a1), s1 = std::sin(a1);

        float x0 = R * c0, z0 = R * s0;
        float x1 = R * c1, z1 = R * s1;

        float nx0 = c0, nz0 = s0;
        float nx1 = c1, nz1 = s1;

        // quad -> two tris, CCW from outside
        push(x0, y0, z0, nx0, 0, nz0);
        push(x1, y0, z1, nx1, 0, nz1);
        push(x1, y1, z1, nx1, 0, nz1);

        push(x0, y0, z0, nx0, 0, nz0);
        push(x1, y1, z1, nx1, 0, nz1);
        push(x0, y1, z0, nx0, 0, nz0);
    }

    // Top cap (+Y) â€” CCW from above: center -> next -> curr
    for (int i = 0; i < slices; ++i) {
        float a0 = (i * 2.f * PI) / slices;
        float a1 = ((i + 1) * 2.f * PI) / slices;
        push(0, y1, 0, 0, 1, 0);
        push(R * std::cos(a1), y1, R * std::sin(a1), 0, 1, 0);
        push(R * std::cos(a0), y1, R * std::sin(a0), 0, 1, 0);
    }

    // Bottom cap (âˆ’Y) â€” CCW from below: center -> curr -> next
    for (int i = 0; i < slices; ++i) {
        float a0 = (i * 2.f * PI) / slices;
        float a1 = ((i + 1) * 2.f * PI) / slices;
        push(0, y0, 0, 0, -1, 0);
        push(R * std::cos(a0), y0, R * std::sin(a0), 0, -1, 0);
        push(R * std::cos(a1), y0, R * std::sin(a1), 0, -1, 0);
    }

    m.vertexCount = static_cast<GLsizei>(m.vertices.size() / 6);
    return m;
}

Mesh Mesh::CreateUnitSphereData(int stacks, int slices) {
    if (stacks < 3) stacks = 3;
    if (slices < 3) slices = 3;

    Mesh m;
    m.vertices.reserve(stacks * slices * 6 * 6);
    constexpr float PI = 3.14159265358979323846f;
    const float R = 0.5f;

    auto push = [&](float x, float y, float z, float nx, float ny, float nz) {
        m.vertices.insert(m.vertices.end(), { x,y,z,nx,ny,nz });
        };

    for (int i = 0; i < stacks; ++i) {
        float th0 = (float(i) / stacks) * PI;
        float th1 = (float(i + 1) / stacks) * PI;

        for (int j = 0; j < slices; ++j) {
            float ph0 = (float(j) / slices) * 2.f * PI;
            float ph1 = (float(j + 1) / slices) * 2.f * PI;

            auto P = [&](float th, float ph) {
                float sx = std::sin(th) * std::cos(ph);
                float sy = std::cos(th);
                float sz = std::sin(th) * std::sin(ph);
                return std::array<float, 6>{ R* sx, R* sy, R* sz, sx, sy, sz };
                };

            auto A = P(th0, ph0);
            auto B = P(th0, ph1);
            auto C = P(th1, ph1);
            auto D = P(th1, ph0);

            // A-B-C
            push(A[0], A[1], A[2], A[3], A[4], A[5]);
            push(B[0], B[1], B[2], B[3], B[4], B[5]);
            push(C[0], C[1], C[2], C[3], C[4], C[5]);
            // A-C-D
            push(A[0], A[1], A[2], A[3], A[4], A[5]);
            push(C[0], C[1], C[2], C[3], C[4], C[5]);
            push(D[0], D[1], D[2], D[3], D[4], D[5]);
        }
    }

    m.vertexCount = static_cast<GLsizei>(m.vertices.size() / 6);
    return m;
}

Mesh Mesh::CreateUnitPlaneXZData() {
    Mesh m;
    const float V[] = {
        // two triangles, CCW, normal +Y
        -0.5f,0.0f,-0.5f, 0,1,0, 0.5f,0.0f,-0.5f, 0,1,0, 0.5f,0.0f, 0.5f, 0,1,0,
        -0.5f,0.0f,-0.5f, 0,1,0, 0.5f,0.0f, 0.5f, 0,1,0,-0.5f,0.0f, 0.5f, 0,1,0,
    };
    m.vertices.assign(std::begin(V), std::end(V));
    m.vertexCount = static_cast<GLsizei>(m.vertices.size() / 6);
    return m;
}

Mesh Mesh::CreateUnitConeData(int slices) 
{
    if (slices < 3) slices = 3;

    Mesh m;
    constexpr float PI = 3.14159265358979323846f;
    const float R = 0.5f;
    const float y0 = -0.5f; // base
    const float y1 = 0.5f; //apex
    auto push = [&](float x,float y,float z,float nx,float ny,float nz)
    {
        m.vertices.insert(m.vertices.end(), {x,y,z,nx,ny,nz});
    };

    auto sideNormal = [&](float c, float s)
    {
        // Tangents: T(a)=(-s,0,c), G(a)=(-R c, 1, -R s)
        float Tx = -s, Ty = 0, Tz = c;
        float Gx = -R * c, Gy = 1, Gz = -R * s;
        // N = T x G, flipped outward
        float nx = -(Ty * Gz - Tz * Gy);// =  c
        float ny = -(Tz * Gx - Tx * Gz);// =  R
        float nz = -(Tx * Gy - Ty * Gx);// =  s
        float len = std::sqrt(nx * nx + ny * ny + nz * nz);
        return std::array<float, 3>{ nx /len, ny / len, nz / len };
    };

    // Sides
    for (int i = 0; i < slices; ++i) 
    {
        float a0 = (i * 2.f * PI) / slices;
        float a1 = ((i + 1) * 2.f * PI) / slices;
        float c0 = std::cos(a0), s0 = std::sin(a0);
        float c1 = std::cos(a1), s1 = std::sin(a1);

        float x0 = R * c0, z0 = R * s0;
        float x1 = R * c1, z1 = R * s1;

        auto N0 = sideNormal(c0, s0);
        auto N1 = sideNormal(c1, s1);
        auto NA = sideNormal((c0 + c1) * 0.5f, (s0 + s1) * 0.5f); // apex normal

        // Triangle: Apex -> P1 -> P0 (CCW from outside)
        push(0, y1, 0, NA[0], NA[1], NA[2]);
        push(x1, y0, z1, N1[0], N1[1], N1[2]);
        push(x0, y0, z0, N0[0], N0[1], N0[2]);
    }

    // Base (âˆ’Y), CCW from below: center -> curr -> next
    for (int i = 0; i < slices; ++i) 
    {
        float a0 = (i * 2.f * PI) / slices;
        float a1 = ((i + 1) * 2.f * PI) / slices;
        push(0, y0, 0, 0, -1, 0);
        push(R * std::cos(a0), y0, R * std::sin(a0), 0, -1, 0);
        push(R * std::cos(a1), y0, R * std::sin(a1), 0, -1, 0);
    }

    m.vertexCount = static_cast<GLsizei>(m.vertices.size() / 6);
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

void Mesh::cleanupGL() 
{
    if (vbo) {
        glDeleteBuffers(1, &vbo);
        vbo = 0;
    }
    if (vao) {
        glDeleteVertexArrays(1, &vao);
        vao = 0;
    }
    vertexCount = 0;
    vertices.clear();
    vertices.shrink_to_fit();
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