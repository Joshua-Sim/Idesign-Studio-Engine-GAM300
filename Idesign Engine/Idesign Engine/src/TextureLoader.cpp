#include "pch.h"
#include "TextureLoader.h"
#include <glew.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <iostream>

unsigned int TextureLoader::loadTexture(TextureProperties& props, bool flip)
{
    int width, height, channels;
    stbi_set_flip_vertically_on_load(flip);

    unsigned char* data = stbi_load(props.path.c_str(), &width, &height, &channels, 0);
    if (!data) {
        std::cerr << "Failed to load texture: " << props.path << std::endl;
        return 0;
    }

    GLenum format = (channels == 1) ? GL_RED :
        (channels == 3) ? GL_RGB :
        (channels == 4) ? GL_RGBA : GL_RGB;

    unsigned int texID;
    

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

    // Store runtime info
    props.textureID = texID;
    props.width = width;
    props.height = height;
    std::cout << "Loading texture: " << props.path << " ID: " << texID << std::endl;
    return props.textureID;
}