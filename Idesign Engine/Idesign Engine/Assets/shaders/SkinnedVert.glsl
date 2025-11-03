// In a new file: "skinned.vert"
#version 430 core

// Must match your VAO setup
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aTex;
layout(location=3) in ivec4 aBoneIDs;  // NEW
layout(location=4) in vec4 aWeights; // NEW

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

// NEW: The array of final bone transforms
#define MAX_BONES 100
uniform mat4 uBoneMatrices[MAX_BONES];

out vec3 vNormal;
out vec3 vWorldPos;
out vec2 vTex;

void main() {
    // Calculate the skinning transformation matrix
    mat4 skinTransform = mat4(0.0);
    
    // This check is important. IDs of -1 mean no bone.
    if(aBoneIDs.x > -1) skinTransform += uBoneMatrices[aBoneIDs.x] * aWeights.x;
    if(aBoneIDs.y > -1) skinTransform += uBoneMatrices[aBoneIDs.y] * aWeights.y;
    if(aBoneIDs.z > -1) skinTransform += uBoneMatrices[aBoneIDs.z] * aWeights.z;
    if(aBoneIDs.w > -1) skinTransform += uBoneMatrices[aBoneIDs.w] * aWeights.w;

    // Apply the skinning transform *before* the model transform
    vec4 localPos = skinTransform * vec4(aPos, 1.0);
    vec4 worldPos = uModel * localPos;

    vWorldPos = worldPos.xyz;
    vTex = aTex;

    // Transform normal by skinning matrix as well
    mat3 normalMatrix = mat3(transpose(inverse(uModel * skinTransform)));
    vNormal = normalize(normalMatrix * aNormal);

    gl_Position = uProj * uView * worldPos;
}