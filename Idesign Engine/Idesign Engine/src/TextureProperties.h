///////////////////////////////////////////////////////////////////////////////
///
///	@file  : TextureProperties.h
/// @Brief : 
///	
///	@Authors: Edwin Leow
///	@Copyright 2025, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#pragma once
#ifndef _TEXTURE_PROPERTIES_H_
#define _TEXTURE_PROPERTIES_H_

#include "PropertySystem.h"
#include "PropertyMacro.h"

namespace Framework
{
    struct TextureProperties
    {
        std::string textureName;
        std::string filePath;
    };

    inline constexpr std::array g_TexturePropertiesPropsArray =
    {
        PROP_STRING(textureName, TextureProperties),
        PROP_STRING(filePath, TextureProperties)
    };

    inline constexpr prop_list g_TexturePropertiesProps{ "TextureProperties", g_TexturePropertiesPropsArray };
    inline global_registration g_TexturePropertiesReg(&g_TexturePropertiesProps);
}
#endif // !_TEXTURE_PROPERTIES_H_