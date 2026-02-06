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

std::unordered_map<std::string, unsigned int> TextureLoader::s_texCache;
unsigned int TextureLoader::loadTexture(TextureProperties& props, bool flip)
{
    //cache first

    if (s_texCache.find(props.path) != s_texCache.end()) {
        props.textureID = s_texCache[props.path];
        return props.textureID;
    }
    // ------------------------------------------------------------------
    // Resolve absolute path
    // ------------------------------------------------------------------
    std::filesystem::path root = GetEngineRootPath();
    std::filesystem::path absolutePath = root / props.path;
    std::string pathStr = absolutePath.string();

    if (!std::filesystem::exists(absolutePath)) {
        std::cerr << "[TextureLoader] File not found: " << pathStr << std::endl;
        return 0;
    }

    // ------------------------------------------------------------------
    // Extension detection
    // ------------------------------------------------------------------
    std::string ext;
    size_t dot = props.path.find_last_of('.');
    if (dot != std::string::npos) {
        ext = props.path.substr(dot + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }

    // ------------------------------------------------------------------
    // DDS handling (must already be OpenGL-safe internally)
    // ------------------------------------------------------------------
    if (ext == "dds") {
        GLuint texID = LoadDDS(pathStr);
        if (texID != 0) {
            props.textureID = texID;
            return texID;
        }
        return 0;
    }

    // ------------------------------------------------------------------
    // Load image via STB
    // ------------------------------------------------------------------
    int width = 0, height = 0, channels = 0;

    stbi_set_flip_vertically_on_load(flip);
    unsigned char* data = stbi_load(
        pathStr.c_str(),
        &width,
        &height,
        &channels,
        0
    );
    stbi_set_flip_vertically_on_load(false);

    if (!data || width <= 0 || height <= 0) {
        std::cerr << "[STB Error] " << stbi_failure_reason()
            << " : " << pathStr << std::endl;
        return 0;
    }

    // ------------------------------------------------------------------
    // Determine OpenGL formats
    // ------------------------------------------------------------------
    GLenum internalFormat = 0;
    GLenum dataFormat = 0;

    switch (channels) {
    case 1:
        internalFormat = GL_R8;
        dataFormat = GL_RED;
        break;
   case 3:
        // Use GL_COMPRESSED_RGB_S3TC_DXT1_EXT for 6:1 compression
        internalFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT; 
        dataFormat = GL_RGB;
        break;
    case 4:
        // Use GL_COMPRESSED_RGBA_S3TC_DXT5_EXT for 4:1 compression with alpha
        internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        dataFormat = GL_RGBA;
        break;
    default:
        std::cerr << "[TextureLoader] Unsupported channel count: "
            << channels << " (" << pathStr << ")" << std::endl;
        stbi_image_free(data);
        return 0;
    }




    // ------------------------------------------------------------------
    // Generate texture object
    // ------------------------------------------------------------------
    GLuint texID = 0;
    glGenTextures(1, &texID);

    if (texID == 0) {
        stbi_image_free(data);
        return 0;
    }

    glBindTexture(GL_TEXTURE_2D, texID);

    // ------------------------------------------------------------------
    // Upload texture data safely
    // ------------------------------------------------------------------
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    while (glGetError() != GL_NO_ERROR);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        internalFormat,
        width,
        height,
        0,
        dataFormat,
        GL_UNSIGNED_BYTE,
        data
    );

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "[OpenGL Error] glTexImage2D failed ("
            << err << ") : " << pathStr << std::endl;
        glDeleteTextures(1, &texID);
        stbi_image_free(data);
        return 0;
    }

    glGenerateMipmap(GL_TEXTURE_2D);

    // ------------------------------------------------------------------
    // Sampler parameters
    // ------------------------------------------------------------------
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // ------------------------------------------------------------------
    // Cleanup & output
    // ------------------------------------------------------------------
    stbi_image_free(data);
    s_texCache[props.path] = texID;
    props.textureID = texID;
    props.width = width;
    props.height = height;

    return texID;
}

