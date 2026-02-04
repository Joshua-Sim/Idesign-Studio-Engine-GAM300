#version 430 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 3) in ivec4 aBoneIDs;
layout(location = 4) in vec4 aWeights;

out vec3 vWorldPos;
uniform mat4 uModel;
uniform mat4 uLightSpaceMatrix;
uniform mat4 uBoneMatrices[100];

void main() {
    mat4 skinTransform = mat4(0.0);
    if (aBoneIDs.x == -1) { skinTransform = mat4(1.0); }
    else {
        skinTransform += uBoneMatrices[aBoneIDs.x] * aWeights.x;
        if (aBoneIDs.y > -1) skinTransform += uBoneMatrices[aBoneIDs.y] * aWeights.y;
        if (aBoneIDs.z > -1) skinTransform += uBoneMatrices[aBoneIDs.z] * aWeights.z;
        if (aBoneIDs.w > -1) skinTransform += uBoneMatrices[aBoneIDs.w] * aWeights.w;
    }

    mat4 modelSkin = uModel * skinTransform;
    mat3 normalMatrix = mat3(transpose(inverse(modelSkin)));
    vec3 worldNormal = normalize(normalMatrix * aNormal);

    vec4 worldPos = modelSkin * vec4(aPos, 1.0);

    // Nudge outward along the mathematically correct normal to clear the black spots
    worldPos.xyz += worldNormal * 0.015;

    vWorldPos = worldPos.xyz;
    gl_Position = uLightSpaceMatrix * worldPos;
}