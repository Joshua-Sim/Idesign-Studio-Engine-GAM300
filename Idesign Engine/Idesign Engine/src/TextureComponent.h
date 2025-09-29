#pragma once
#include <string>
#include <glew.h>

struct TextureComponent {
    GLuint textureID = 0;          // OpenGL handle
    std::string path = "Assets/PNG/Airplane_2_tex.jpg";              // Texture file path for loading
};
