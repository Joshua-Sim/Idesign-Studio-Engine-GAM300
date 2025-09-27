#pragma once
#include <string>
#include <glew.h>

struct TextureComponent {
    GLuint textureID = 0;          // OpenGL handle
    std::string path;              // Texture file path for loading
};
