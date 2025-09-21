#version 430 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aTex;   // UVs

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec3 vNormal;
out vec3 vWorldPos;
out vec2 vTex;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;

    mat3 N = mat3(transpose(inverse(uModel)));
    vNormal = normalize(N * aNormal);

    vTex = aTex;
    gl_Position = uProj * uView * worldPos;
}
