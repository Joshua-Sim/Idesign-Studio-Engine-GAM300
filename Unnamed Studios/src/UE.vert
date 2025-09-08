///////////////////////////////////////////////////////////////////////////////
///
///	@file UE.vert
/// 
/// @brief Fragment files for shader
///	
///	@Authors: Victor lee
///	Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////



#version 450 core


layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texCoord;

uniform mat4 modelMatrix; // Transformation matrix from CPU
uniform mat4 viewMatrix;  // Usually identity in 2D
uniform mat4 projectionMatrix; // Typically an orthographic projection in 2D

layout(location = 0)out vec2 TexCoord;

void main()
{
    gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(position, 0.0, 1.0);
    TexCoord = texCoord;
}

