#pragma once
#include <string>

struct MeshComponent
{
    std::string path = "Assets/Objects/plane2.obj"; // asset path to load and fill in mesh component vars, alloted default
    unsigned int vao = 0, vbo = 0, ebo = 0;
    unsigned int indexCount = 0;
};
