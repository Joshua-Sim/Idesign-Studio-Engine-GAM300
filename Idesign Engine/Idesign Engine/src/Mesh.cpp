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

Mesh Mesh::CreateUnitCylinderData(int segments)
{
    Mesh mesh;
    std::vector<float> verts;

    const float radius = 0.5f;
    const float height = 1.0f;
    const float halfH = height * 0.5f;

    const float TWO_PI = 6.28318530718f;

    // ---------------------------------------
    // SIDE VERTICES (NO INDEX BUFFER)
    // ---------------------------------------

    for (int i = 0; i < segments; i++)
    {
        float t0 = (float)i / segments;
        float t1 = (float)(i + 1) / segments;

        float th0 = t0 * TWO_PI;
        float th1 = t1 * TWO_PI;

        float x0 = cosf(th0), z0 = sinf(th0);
        float x1 = cosf(th1), z1 = sinf(th1);

        float px0 = x0 * radius, pz0 = z0 * radius;
        float px1 = x1 * radius, pz1 = z1 * radius;

        // Quad split into 2 triangles:
        // Triangle 1 (bottom0, top0, top1)
        // Triangle 2 (bottom0, top1, bottom1)

        // bottom0
        verts.insert(verts.end(), { px0, -halfH, pz0,   x0, 0.0f, z0,    t0, 0.0f });
        // top0
        verts.insert(verts.end(), { px0,  halfH, pz0,   x0, 0.0f, z0,    t0, 1.0f });
        // top1
        verts.insert(verts.end(), { px1,  halfH, pz1,   x1, 0.0f, z1,    t1, 1.0f });

        // bottom0
        verts.insert(verts.end(), { px0, -halfH, pz0,   x0, 0.0f, z0,    t0, 0.0f });
        // top1
        verts.insert(verts.end(), { px1,  halfH, pz1,   x1, 0.0f, z1,    t1, 1.0f });
        // bottom1
        verts.insert(verts.end(), { px1, -halfH, pz1,   x1, 0.0f, z1,    t1, 0.0f });
    }

    // ---------------------------------------
    // TOP CAP (triangle fan, no index buffer)
    // ---------------------------------------
    const float nx_top = 0.f;
    const float ny_top = 1.f;
    const float nz_top = 0.f;

    float centerU = 0.5f;
    float centerV = 0.5f;

    for (int i = 0; i < segments; i++)
    {
        float t0 = (float)i / segments;
        float t1 = (float)(i + 1) / segments;

        float th0 = t0 * TWO_PI;
        float th1 = t1 * TWO_PI;

        float x0 = cosf(th0), z0 = sinf(th0);
        float x1 = cosf(th1), z1 = sinf(th1);

        // center
        verts.insert(verts.end(), {
            0.f, halfH, 0.f,
            nx_top, ny_top, nz_top,
            centerU, centerV
            });

        // edge0
        verts.insert(verts.end(), {
            x0 * radius, halfH, z0 * radius,
            nx_top, ny_top, nz_top,
            (x0 * 0.5f + 0.5f), (z0 * 0.5f + 0.5f)
            });

        // edge1
        verts.insert(verts.end(), {
            x1 * radius, halfH, z1 * radius,
            nx_top, ny_top, nz_top,
            (x1 * 0.5f + 0.5f), (z1 * 0.5f + 0.5f)
            });
    }

    // ---------------------------------------
    // BOTTOM CAP
    // ---------------------------------------
    const float nx_bot = 0.f;
    const float ny_bot = -1.f;
    const float nz_bot = 0.f;

    for (int i = 0; i < segments; i++)
    {
        float t0 = (float)i / segments;
        float t1 = (float)(i + 1) / segments;

        float th0 = t0 * TWO_PI;
        float th1 = t1 * TWO_PI;

        float x0 = cosf(th0), z0 = sinf(th0);
        float x1 = cosf(th1), z1 = sinf(th1);

        // center
        verts.insert(verts.end(), {
            0.f, -halfH, 0.f,
            nx_bot, ny_bot, nz_bot,
            centerU, centerV
            });

        // edge1  (reversed order for correct winding)
        verts.insert(verts.end(), {
            x1 * radius, -halfH, z1 * radius,
            nx_bot, ny_bot, nz_bot,
            (x1 * 0.5f + 0.5f), (z1 * 0.5f + 0.5f)
            });

        // edge0
        verts.insert(verts.end(), {
            x0 * radius, -halfH, z0 * radius,
            nx_bot, ny_bot, nz_bot,
            (x0 * 0.5f + 0.5f), (z0 * 0.5f + 0.5f)
            });
    }

    mesh.vertices = verts;
    mesh.vertexCount = (GLsizei)(verts.size() / 8);
    return mesh;
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
    auto push = [&](float x, float y, float z, float nx, float ny, float nz)
        {
            m.vertices.insert(m.vertices.end(), { x,y,z,nx,ny,nz });
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
            return std::array<float, 3>{ nx / len, ny / len, nz / len };
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
    glEnableVertexAttribArray(0); // pos
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(1); // normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));

    glEnableVertexAttribArray(2); // uv
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));

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

Mesh Mesh::CreateUnitTriangleData()
{
    Mesh triangle;
    triangle.vertexCount = 12; // 4 faces * 3 vertices per face

    // Define the 4 vertices of the tetrahedron
    glm::vec3 p0(0.0f, 1.0f, 0.0f);
    glm::vec3 p1(0.0f, -1.0f / 3.0f, 2.0f * sqrt(2.0f) / 3.0f);
    glm::vec3 p2(-sqrt(6.0f) / 3.0f, -1.0f / 3.0f, -sqrt(2.0f) / 3.0f);
    glm::vec3 p3(sqrt(6.0f) / 3.0f, -1.0f / 3.0f, -sqrt(2.0f) / 3.0f);
    // Calculate the normal for each face (pointing outwards)
    // The normal is found by taking the cross product of two edges of the triangle
    glm::vec3 n0 = glm::normalize(glm::cross(p2 - p1, p3 - p1)); // Bottom face
    glm::vec3 n1 = glm::normalize(glm::cross(p3 - p0, p2 - p0)); // Front-left face
    glm::vec3 n2 = glm::normalize(glm::cross(p1 - p0, p3 - p0)); // Front-right face
    glm::vec3 n3 = glm::normalize(glm::cross(p2 - p0, p1 - p0)); // Back face

    triangle.vertices = {
        // --- Bottom Face --- (p1, p2, p3)
        // Position          // Normal
        p1.x, p1.y, p1.z,      n0.x, n0.y, n0.z,
        p2.x, p2.y, p2.z,      n0.x, n0.y, n0.z,
        p3.x, p3.y, p3.z,      n0.x, n0.y, n0.z,

        // --- Front-Left Face --- (p0, p2, p3)
        p0.x, p0.y, p0.z,      n1.x, n1.y, n1.z,
        p2.x, p2.y, p2.z,      n1.x, n1.y, n1.z,
        p3.x, p3.y, p3.z,      n1.x, n1.y, n1.z,

        // --- Front-Right Face --- (p0, p3, p1)
        p0.x, p0.y, p0.z,      n2.x, n2.y, n2.z,
        p3.x, p3.y, p3.z,      n2.x, n2.y, n2.z,
        p1.x, p1.y, p1.z,      n2.x, n2.y, n2.z,

        // --- Back Face --- (p0, p1, p2)
        p0.x, p0.y, p0.z,      n3.x, n3.y, n3.z,
        p1.x, p1.y, p1.z,      n3.x, n3.y, n3.z,
        p2.x, p2.y, p2.z,      n3.x, n3.y, n3.z
    };

    return triangle;
}

Mesh Mesh::CreateBillboardQuad()
{
    Mesh m;
    m.vertices = {
        // pos            normal         uv
        -0.5f, 0.f, 0.f,   0.f, 0.f, 1.f,   0.f, 0.f,
         0.5f, 0.f, 0.f,   0.f, 0.f, 1.f,   1.f, 0.f,
         0.5f, 1.f, 0.f,   0.f, 0.f, 1.f,   1.f, 1.f,

        -0.5f, 0.f, 0.f,   0.f, 0.f, 1.f,   0.f, 0.f,
         0.5f, 1.f, 0.f,   0.f, 0.f, 1.f,   1.f, 1.f,
        -0.5f, 1.f, 0.f,   0.f, 0.f, 1.f,   0.f, 1.f,
    };

    m.vertexCount = 6;
    return m;
}


const float* Mesh::GetRawVertexData() const {
    return vertices.data();
}

size_t Mesh::GetRawDataSize() const {
    return vertices.size();
}

unsigned int Mesh::GetVertexCount() const {
    return vertexCount;
}