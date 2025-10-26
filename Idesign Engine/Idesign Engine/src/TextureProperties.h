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
        std::string guid;
        std::string name;
        std::string path;
        bool isDirty = false;

        // Runtime-only
        unsigned int textureID = 0;
        int width = 0;
        int height = 0;
    };

    /*
    std::vector<std::string> GetAvailableTextureNames()
    {
        std::vector<std::string> names;
        for (auto guid : GetAllTextureGUIDs())
        {
            const auto& tex = ecs.GetComponent<TextureProperties>(guid);
            names.push_back(tex.name); // or tex.guid, or tex.filePath
        }
        return names;
    }

    
    inline constexpr std::array g_TexturePropsArray =
    {
        PROP_STRING_DROPDOWN_DYNAMIC(textureName, TextureProperties, GetAvailableTextureNames),
    };
    */

    inline const std::vector g_TexturePropertiesPropsArray =
    {
        PROP_STRING_READONLY(guid, TextureProperties),
        PROP_STRING(name, TextureProperties),
        PROP_STRING_READONLY(path, TextureProperties),
    };

    inline const prop_list g_TexturePropertiesProps{ "TextureProperties", g_TexturePropertiesPropsArray };
    inline global_registration g_TexturePropertiesReg(&g_TexturePropertiesProps);
}
#endif // !_TEXTURE_PROPERTIES_H_