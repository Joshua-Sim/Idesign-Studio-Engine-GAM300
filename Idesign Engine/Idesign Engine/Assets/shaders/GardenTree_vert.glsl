#version 430 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

layout(location = 3) in vec4 iModel0;
layout(location = 4) in vec4 iModel1;
layout(location = 5) in vec4 iModel2;
layout(location = 6) in vec4 iModel3;
layout(location = 7) in float aOpacity;
layout(location = 8) in float aVariation;

uniform mat4 uView;
uniform mat4 uProj;
uniform mat4 uModel;

out vec2 TexCoord;
out float vOpacity;
out float vVariation;

void main()
{
    mat4 instanceModel = mat4(iModel0, iModel1, iModel2, iModel3);

    TexCoord = aTexCoord;
    vOpacity = aOpacity;
    vVariation = aVariation;

    gl_Position = uProj * uView * uModel * instanceModel * vec4(aPos, 1.0);
}