#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D uTexture; // Your 2D image
uniform vec3 uColor;        // Optional: To tint the image (usually white)
uniform float uAlpha;       // Optional: To fade the UI (transparency)

void main()
{
    vec4 texColor = texture(uTexture, TexCoords);
    
    // Apply tint and global alpha
    FragColor = vec4(texColor.rgb * uColor, texColor.a * uAlpha);
    
    // Optimization: Don't draw invisible pixels
    if(FragColor.a < 0.01)
        discard;

    // Gamma correction: convert linear -> sRGB for correct brightness
    // sRGB textures are auto-converted to linear on sample, so we convert back
    FragColor.rgb = pow(FragColor.rgb, vec3(1.0 / 1.5));
}
