#pragma once
#ifndef _TEXT_COMPONENT_H_
#define _TEXT_COMPONENT_H_

#include "PropertySystem.h" 
#include "PropertyMacro.h"  
#include <glm.hpp>

namespace Framework
{
    // Define the enum for clarity
    enum class TextAlignment {
        Left = 0,
        Center = 1,
        Right = 2
    };

    struct TextProperties
    {
        std::string text = "Hello World";
        float scale = 0.05f;
        glm::vec3 color = { 1.0f, 1.0f, 1.0f };

        // [NEW] 0=Left, 1=Center, 2=Right
        int alignment = 0;
    };

    // 1. Define an array that lists each property
    inline const std::array g_TextComponentPropsArray =
    {
        PROP_STRING(text, TextProperties),
        PROP_DRAG_FLOAT(scale, TextProperties),
        PROP_COLOR3(color, TextProperties),

        // [NEW] Register the integer. 
        // Note: If your property system doesn't support Enums directly, 
        // we register it as an INT here, and handle the Dropdown in the Editor logic (Step 2).
        PROP_INT(alignment, TextProperties)
    };

    inline const prop_list g_TextComponentProps{ "TextProperties", g_TextComponentPropsArray };
    inline global_registration g_TextComponentReg(&g_TextComponentProps);
}
#endif // !_TEXT_COMPONENT_H_