///////////////////////////////////////////////////////////////////////////////
///
///	@file  : WindowConfigProperties.h
/// @Brief : 
///	
///	@Authors: Edwin Leow
///	@Copyright 2025, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#pragma once
#ifndef _WINDOW_CONFIG_PROPERTIES_H_
#define _WINDOW_CONFIG_PROPERTIES_H_

#include "PropertySystem.h"
#include "PropertyMacro.h"

namespace Framework
{
    struct WindowConfig
    {
        int x = 0;
        int y = 0;
        std::string programName;
    };

    inline constexpr std::array g_WindowConfigPropsArray =
    {
        PROP_INT_META(x, WindowConfig, "Horizontal position", "Position", 0, 1920),
        PROP_INT_META(y, WindowConfig, "Vertical position", "Position", 0, 1080),
        PROP_STRING_META(programName, WindowConfig, "Name of the window", "General"),
    };

    inline constexpr prop_list g_WindowConfigProps{ "WindowConfig", g_WindowConfigPropsArray };
    inline global_registration g_WindowConfigReg(&g_WindowConfigProps);
}
#endif // !_WINDOW_CONFIG_PROPERTIES_H_