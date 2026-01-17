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
        AssetHandle guid;
        std::string name;
        std::string path;
        bool isUI = false;

        // Runtime-only
        unsigned int textureID = 0;
        int width = 0;
        int height = 0;
    };

    inline const std::vector g_TexturePropertiesPropsArray =
    {
        PROP_GUID_HIDDEN(guid, TextureProperties),
        PROP_STRING_HIDDEN(name, TextureProperties),
        PROP_STRING_HIDDEN(path, TextureProperties),
        PROP_BOOL(isUI, TextureProperties)
    };

    inline const prop_list g_TexturePropertiesProps{ "TextureProperties", g_TexturePropertiesPropsArray };
    inline global_registration g_TexturePropertiesReg(&g_TexturePropertiesProps);
}
#endif // !_TEXTURE_PROPERTIES_H_