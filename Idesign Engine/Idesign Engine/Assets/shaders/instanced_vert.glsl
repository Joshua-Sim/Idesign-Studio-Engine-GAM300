#version 430 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
// Locations 3, 4, 5, 6 are reserved for the 4x4 Matrix
layout (location = 3) in mat4 aInstanceMatrix; 

out vec2 TexCoords;
out vec3 Normal;

uniform mat4 uProj;
uniform mat4 uView;

void main()
{
    TexCoords = aTexCoords;
    
    // Use the instance matrix directly
    gl_Position = uProj * uView * aInstanceMatrix * vec4(aPos, 1.0);
    
    // Rotate normal by the instance rotation (upper 3x3 of matrix)
    Normal = mat3(transpose(inverse(aInstanceMatrix))) * aNormal; 
}