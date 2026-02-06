#pragma once
#include <string>
#include "TextureProperties.h"

using namespace Framework;

class TextureLoader 
{
public:
    static unsigned int loadTexture(TextureProperties& props, bool flip);
private:
    // Global cache mapping file paths to their OpenGL Texture IDs
    static std::unordered_map<std::string, unsigned int> s_texCache;
};
