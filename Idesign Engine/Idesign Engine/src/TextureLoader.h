#pragma once
#include <string>
#include "TextureProperties.h"

using namespace Framework;

class TextureLoader 
{
public:
    static unsigned int loadTexture(TextureProperties& props, bool flip);
};
