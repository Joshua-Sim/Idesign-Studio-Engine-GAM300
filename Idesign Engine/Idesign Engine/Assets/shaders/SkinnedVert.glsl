#version 430 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTex;
layout(location = 3) in ivec4 aBoneIDs;
layout(location = 4) in vec4 aWeights;

out vec3 vNormal;
out vec3 vWorldPos;
out vec2 vTex;
out vec4 v_FragPosLightSpace;

uniform mat4 uProj;
uniform mat4 uView;
uniform mat4 uModel;
uniform mat4 uBoneMatrices[100];
uniform mat4 uLightSpaceMatrix;

void main() {
    mat4 skinTransform = mat4(0.0);
    if (aBoneIDs.x == -1) {
        skinTransform = mat4(1.0);
    }
    else {
        skinTransform += uBoneMatrices[aBoneIDs.x] * aWeights.x;
        if (aBoneIDs.y > -1) skinTransform += uBoneMatrices[aBoneIDs.y] * aWeights.y;
        if (aBoneIDs.z > -1) skinTransform += uBoneMatrices[aBoneIDs.z] * aWeights.z;
        if (aBoneIDs.w > -1) skinTransform += uBoneMatrices[aBoneIDs.w] * aWeights.w;
    }

    // --- FIX: COMBINED NORMAL MATRIX ---
    // Multiply model and skin together FIRST, then take inverse-transpose
    mat4 modelSkin = uModel * skinTransform;
    mat3 normalMatrix = mat3(transpose(inverse(modelSkin)));

    vec4 worldPos = modelSkin * vec4(aPos, 1.0);

    vNormal = normalize(normalMatrix * aNormal);
    vWorldPos = worldPos.xyz;
    vTex = aTex;
    v_FragPosLightSpace = uLightSpaceMatrix * worldPos;
    gl_Position = uProj * uView * worldPos;
}