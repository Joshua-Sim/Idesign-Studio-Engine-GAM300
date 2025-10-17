/******************************************************************************/
/*!
\file		MainMath.cpp
\author 	Jason Yeo Chee Kang, y.jasoncheekang, 2301505
\par    	email: y.jasoncheekang@digipen.edu
\date   	October 5, 2025
\brief		This file implements the declarations for any math function to be use.
            Acting as the main math library to be use accross.

Copyright (C) 2024 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
 /******************************************************************************/

#pragma once

#include "Vector2D.h"
#include "Vector3D.h"
#include "Matrix3x3.h"
#include "Matrix4x4.h"
#include <cmath>

namespace UE_Math {
    const float PI = 3.14159265359f;
    const float DEG2RAD = PI / 180.0f;
    const float RAD2DEG = 180.0f / PI;

    template<typename T>
    inline T Clamp(T value, T min, T max) {
        if (value < min) return min;
        if (value > max) return max;
        return value;
    }

    template<typename T>
    inline T Lerp(const T& a, const T& b, float t) {
        return a + (b - a) * t;
    }

    inline float DegToRad(float degrees) { return degrees * DEG2RAD; }
    inline float RadToDeg(float radians) { return radians * RAD2DEG; }
}