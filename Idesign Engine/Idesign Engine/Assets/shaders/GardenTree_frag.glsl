#version 430 core

in vec2 TexCoord;
in float vOpacity;
in float vVariation;

uniform sampler2D uTexture;
uniform vec3 uAmbientColor;

out vec4 FragColor;

void main()
{
    vec4 tex = texture(uTexture, TexCoord);

    if (tex.a < 0.1)
        discard;

    float brightness = 0.85 + vVariation * 0.15;
    vec3 finalColor = tex.rgb * uAmbientColor * brightness;

    FragColor = vec4(finalColor, tex.a * vOpacity);
}