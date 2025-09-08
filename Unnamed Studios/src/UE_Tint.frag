///////////////////////////////////////////////////////////////////////////////
///
///	@file UE_color.frag
/// 
/// @brief Fragment shader for handling solid colors
///	
///	@Authors: Victor Lee
///	Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////

#version 450 core

// Output fragment color
layout (location=0) out vec4 fFragColor;

// Color uniform
uniform vec3 uColor;

// Alpha uniform
uniform float uAlpha;  // Value between 0.0 (fully transparent) and 1.0 (fully opaque)

void main()
{
    // Set the fragment color to the provided color and alpha
    fFragColor = vec4(uColor, uAlpha);
}
