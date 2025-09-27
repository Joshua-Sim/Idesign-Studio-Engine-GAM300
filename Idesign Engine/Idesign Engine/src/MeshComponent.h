#pragma once
#include <string>

struct MeshComponent
{
    std::string path; // asset path to load and fill in mesh component vars
    unsigned int vao = 0, vbo = 0, ebo = 0;
    unsigned int indexCount = 0;
};
