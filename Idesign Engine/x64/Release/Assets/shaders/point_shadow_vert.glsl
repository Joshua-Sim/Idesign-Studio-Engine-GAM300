#version 430 core

// INPUT: From your Vertex Array Object (VAO)
layout(location=0) in vec3 aPos;

// OUTPUT: To the point_shadow_frag.glsl
out vec3 vWorldPos;

// UNIFORMS: From your Graphics.cpp
uniform mat4 uModel;
uniform mat4 uLightSpaceMatrix;

void main()
{
    // 1. Calculate the full vec4 world position
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    // 2. Pass the xyz part to the fragment shader
    vWorldPos = worldPos.xyz;
    // 3. Pass the *full* vec4 (including its 'w') to the projection matrix
    gl_Position = uLightSpaceMatrix * worldPos;
}