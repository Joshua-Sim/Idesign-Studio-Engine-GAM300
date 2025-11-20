#include "pch.h"
#include "TextureLoader.h"
#include <glew.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <iostream>
#include <algorithm> // for transform
#include "TextureLoaderDDS.h"
#include "EnginePath.h" 
#include <filesystem>

unsigned int TextureLoader::loadTexture(TextureProperties& props, bool flip)
{
    // <-- FIX: Build the absolute path from the relative path
    std::filesystem::path root = GetEngineRootPath();
    std::filesystem::path absolutePath = root / props.path;
    std::string pathStr = absolutePath.string(); // Use this full path for loading

    // Detect file extension (lowercase)
    std::string ext;
    size_t dot = props.path.find_last_of('.'); // Use props.path (relative) for extension
    if (dot != std::string::npos)
    {
        ext = props.path.substr(dot + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }

    // 1. DDS compressed path
    if (ext == "dds")
    {
        GLuint texID = LoadDDS(pathStr); // <-- FIX: Use full path string
        if (texID)
        {
            props.textureID = texID;
            // width/height optional if your loader returns them
            std::cout << "[TextureLoader] Loaded compressed DDS: " << pathStr << std::endl;
            return texID;
        }
        std::cerr << "[TextureLoader] Failed to load DDS: " << pathStr << std::endl;
        return 0;
    }

    // 2. Default (STB path)
    int width, height, channels;
    stbi_set_flip_vertically_on_load(flip);

    // <-- FIX: Use the .c_str() of the full path string
    unsigned char* data = stbi_load(pathStr.c_str(), &width, &height, &channels, 0);
    if (!data)
    {
        // <-- FIX: Use full path in error message
        std::cerr << "Failed to load texture: " << pathStr << std::endl;
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

    //std::cout << "[TextureLoader] Loaded raw image: " << pathStr << std::endl;
    return texID;
}
