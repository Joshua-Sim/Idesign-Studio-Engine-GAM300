#version 430 core

layout (location = 0) in vec3 aPos;
layout (location = 2) in vec2 aUV;
layout (location = 3) in mat4 aInstance;

out vec2 TexCoords;

uniform mat4 uProj;
uniform mat4 uView;

// Extract camera rotation only (for billboard)
mat3 ExtractCameraRotation(mat4 view)
{
    mat3 rot;
    rot[0] = normalize(vec3(view[0][0], view[1][0], view[2][0]));
    rot[1] = normalize(vec3(view[0][1], view[1][1], view[2][1]));
    rot[2] = normalize(vec3(view[0][2], view[1][2], view[2][2]));
    return rot;
}

void main()
{
    TexCoords = aUV;

    // Extract translation
    vec3 posWS = vec3(aInstance[3]);

    // Read encoded scale
    float sx = aInstance[0][3];
    float sy = aInstance[1][3];

    mat3 camRot = ExtractCameraRotation(uView);

    vec3 billboardPos =
        posWS +
        camRot * vec3(aPos.x * sx, aPos.y * sy, 0.0);

    gl_Position = uProj * uView * vec4(billboardPos, 1.0);
}
