#include "pch.h"
#include "TextureLoader.h"
#include <glew.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <iostream>
#include <algorithm> // for transform
#include "TextureLoaderDDS.h"

unsigned int TextureLoader::loadTexture(TextureProperties& props, bool flip)
{
    // Detect file extension (lowercase)
    std::string path = props.path;
    std::string ext;
    size_t dot = path.find_last_of('.');
    if (dot != std::string::npos)
    {
        ext = path.substr(dot + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }

    // 1. DDS compressed path
    if (ext == "dds")
    {
        GLuint texID = LoadDDS(path);
        if (texID)
        {
            props.textureID = texID;
            std::cout << "[TextureLoader] Loaded compressed DDS: " << path << std::endl;
            return texID;
        }
        std::cerr << "[TextureLoader] Failed to load DDS: " << path << std::endl;
        return 0;
    }

    // 2. Default (STB path)
    int width, height, channels;
    stbi_set_flip_vertically_on_load(flip);
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
    if (!data)
    {
        std::cerr << "Failed to load texture: " << path << std::endl;
        return 0;
    }

    GLenum format = (channels == 1) ? GL_RED :
        (channels == 3) ? GL_RGB : GL_RGBA;

    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0,
        format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);

    props.textureID = texID;
    props.width = width;
    props.height = height;

    std::cout << "[TextureLoader] Loaded raw image: " << path << std::endl;
    return texID;
}