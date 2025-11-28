#version 430 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D text;
uniform vec3 textColor;
uniform float opacity;
void main()
{
  // Get alpha from font texture (Red channel)
    float fontAlpha = texture(text, TexCoord).r;
    
    // Combine font alpha with global opacity
    FragColor = vec4(textColor, fontAlpha * opacity);
}