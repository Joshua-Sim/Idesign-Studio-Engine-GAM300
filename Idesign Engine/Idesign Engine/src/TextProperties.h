#pragma once
#ifndef _TEXT_COMPONENT_H_
#define _TEXT_COMPONENT_H_

#include "PropertySystem.h" 
#include "PropertyMacro.h"  
#include <glm.hpp>

namespace Framework
{
    struct TextProperties {
        std::string text = "Hello World";
        float scale = 0.05f;
        glm::vec3 color = { 1.0f, 1.0f, 1.0f };
    };

    // 1. Define an array that lists each property in the struct
    inline const std::array g_TextComponentPropsArray =
    {
        PROP_STRING(text, TextProperties),
        PROP_FLOAT(scale, TextProperties),
        PROP_VEC3(color, TextProperties)
    };

    inline const prop_list g_TextComponentProps{ "TextProperties", g_TextComponentPropsArray };

    inline global_registration g_TextComponentReg(&g_TextComponentProps);
}

#endif // !_TEXT_COMPONENT_H_