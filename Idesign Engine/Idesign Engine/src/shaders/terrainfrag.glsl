#version 430 core
out vec4 FragColor;

in vec3 vNormal;
in vec3 vWorldPos;

struct DirectionalLight {
    vec3 direction;
    vec3 color;
    float intensity;
};

struct PointLight {
    vec3 position;
    vec3 color;
    float intensity;
    float radius;
};

struct SpotLight {
    vec3 position;
    vec3 direction;
    vec3 color;
    float intensity;
    float radius;
    float innerCutoff;
    float outerCutoff;
};

#define MAX_POINT_LIGHTS 8
#define MAX_SPOT_LIGHTS  4

uniform DirectionalLight uDirLight;
uniform bool uHasDirLight;

uniform int uNumPointLights;
uniform PointLight uPointLights[MAX_POINT_LIGHTS];

uniform int uNumSpotLights;
uniform SpotLight uSpotLights[MAX_SPOT_LIGHTS];

uniform vec3 uAlbedo;

void main() {
    vec3 N = normalize(vNormal);
    vec3 result = vec3(0.1); // baseline ambient

    // Directional
    if (uHasDirLight && uDirLight.intensity > 0.0) {
        vec3 Ld = normalize(-uDirLight.direction);
        float diff = max(dot(N, Ld), 0.0);
        result += uDirLight.color * uDirLight.intensity * diff;
    }

    // Point lights
    for (int i = 0; i < uNumPointLights; ++i) {
        if (uPointLights[i].intensity <= 0.0 || uPointLights[i].radius <= 0.0) continue;

        vec3 L = uPointLights[i].position - vWorldPos;
        float dist = length(L);
        if (dist < uPointLights[i].radius) {
            vec3 Lnorm = normalize(L);
            float d = max(dot(N, Lnorm), 0.0);
            float atten = 1.0 - (dist / uPointLights[i].radius);
            result += uPointLights[i].color * uPointLights[i].intensity * d * atten;
        }
    }

    // Spot lights
    for (int i = 0; i < uNumSpotLights; ++i) {
        if (uSpotLights[i].intensity <= 0.0 || uSpotLights[i].radius <= 0.0) continue;

        vec3 L = uSpotLights[i].position - vWorldPos;
        float dist = length(L);
        if (dist < uSpotLights[i].radius) {
            vec3 Lnorm = normalize(L);
            float theta = dot(Lnorm, normalize(-uSpotLights[i].direction));
            float epsilon = uSpotLights[i].innerCutoff - uSpotLights[i].outerCutoff;
            float spotFactor = clamp((theta - uSpotLights[i].outerCutoff) / epsilon, 0.0, 1.0);

            float d = max(dot(N, Lnorm), 0.0);
            float atten = 1.0 - (dist / uSpotLights[i].radius);
            result += uSpotLights[i].color * uSpotLights[i].intensity * d * atten * spotFactor;
        }
    }

    FragColor = vec4(uAlbedo * result, 1.0);
}
