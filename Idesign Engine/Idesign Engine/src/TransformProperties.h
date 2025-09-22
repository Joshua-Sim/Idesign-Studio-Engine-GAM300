///////////////////////////////////////////////////////////////////////////////
///
///	@file  : TransformProperties.h
/// @Brief : 
///	
///	@Authors: Edwin Leow
///	@Copyright 2025, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#pragma once
#ifndef _TRANSFORM_PROPERTIES_H_
#define _TRANSFORM_PROPERTIES_H_

#include "PropertySystem.h"
#include "PropertyMacro.h"

namespace Framework
{
    struct TransformComponent
    {
        float x = 0.0f;
        float y = 0.0f;
        float rotation = 0.0f;
        float scale = 1.0f;
    };

    inline constexpr std::array g_TransformPropsArray =
    {
        PROP_FLOAT_META(x, TransformComponent, "X position", "Position", -1000.0f, 1000.0f),
        PROP_FLOAT_META(y, TransformComponent, "Y position", "Position", -1000.0f, 1000.0f),
        PROP_FLOAT_META(rotation, TransformComponent, "Rotation in degrees", "Transform", 0.0f, 360.0f),
        PROP_FLOAT_META(scale, TransformComponent, "Uniform scale factor", "Transform", 0.1f, 10.0f),
    };

    inline constexpr prop_list g_TransformProps{ "TransformComponent", g_TransformPropsArray };
    inline global_registration g_TransformPropsReg(&g_TransformProps);
}
#endif // !_TRANSFORM_PROPERTIES_H_
