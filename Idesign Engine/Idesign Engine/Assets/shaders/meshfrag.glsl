// In: meshfrag.glsl

#version 430 core
out vec4 FragColor;

in vec3 vNormal;
in vec3 vWorldPos;
in vec2 vTex;

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

// --- NEW MATERIAL STRUCT ---
struct Material {
    vec3 ambient;   // Ambient reflectance
    vec3 specular;  // Specular reflectance
    float shininess; // Shininess exponent
    float opacity;   // transparent
};
// -------------------------

#define MAX_POINT_LIGHTS 8
#define MAX_SPOT_LIGHTS  4

uniform DirectionalLight uDirLight;
uniform bool uHasDirLight;

uniform int uNumPointLights;
uniform PointLight uPointLights[MAX_POINT_LIGHTS];

uniform int uNumSpotLights;
uniform SpotLight uSpotLights[MAX_SPOT_LIGHTS];

uniform vec3 uAlbedo; // This will come from MeshProperties.albedo (or highlight color)
uniform sampler2D uTexture;
uniform vec3 uViewPos; // We need the camera position for specular
uniform Material uMaterial; // New material uniforms

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uViewPos - vWorldPos); // View direction

    // --- We will sum diffuse and specular components separately ---
    vec3 totalDiffuse = vec3(0.0);
    vec3 totalSpecular = vec3(0.0);
    // -----------------------------------------------------------

    // Lighting (updated for specular)
    if (uHasDirLight && uDirLight.intensity > 0.0) {
        vec3 Ld = normalize(-uDirLight.direction);
        
        // Diffuse
        float diff = max(dot(N, Ld), 0.0);
        totalDiffuse += uDirLight.color * uDirLight.intensity * diff;
        
        // Specular (Blinn-Phong)
        vec3 H = normalize(Ld + V);
        float spec = pow(max(dot(N, H), 0.0), uMaterial.shininess);
        totalSpecular += uMaterial.specular * spec * uDirLight.color * uDirLight.intensity;
    }

    for (int i = 0; i < uNumPointLights; ++i) {
        if (uPointLights[i].intensity <= 0.0 || uPointLights[i].radius <= 0.0) continue;
        
        vec3 L = uPointLights[i].position - vWorldPos;
        float dist = length(L);
        
        if (dist < uPointLights[i].radius) {
            vec3 Lnorm = normalize(L);
            float atten = 1.0 - (dist / uPointLights[i].radius);
            
            // Diffuse
            float d = max(dot(N, Lnorm), 0.0);
            totalDiffuse += uPointLights[i].color * uPointLights[i].intensity * d * atten;

            // Specular (Blinn-Phong)
            vec3 H = normalize(Lnorm + V);
            float spec = pow(max(dot(N, H), 0.0), uMaterial.shininess);
            totalSpecular += uMaterial.specular * spec * uPointLights[i].color * uPointLights[i].intensity * atten;
        }
    }

    for (int i = 0; i < uNumSpotLights; ++i) {
        if (uSpotLights[i].intensity <= 0.0 || uSpotLights[i].radius <= 0.0) continue;
        
        vec3 L = uSpotLights[i].position - vWorldPos;
        float dist = length(L);
        
        if (dist < uSpotLights[i].radius) {
            vec3 Lnorm = normalize(L);
            float theta = dot(Lnorm, normalize(-uSpotLights[i].direction));
            float epsilon = uSpotLights[i].innerCutoff - uSpotLights[i].outerCutoff;
            float spotFactor = clamp((theta - uSpotLights[i].outerCutoff) / epsilon, 0.0, 1.0);
            float atten = 1.0 - (dist / uSpotLights[i].radius);
            
            // Diffuse
            float d = max(dot(N, Lnorm), 0.0);
            totalDiffuse += uSpotLights[i].color * uSpotLights[i].intensity * d * atten * spotFactor;

            // Specular (Blinn-Phong)
            vec3 H = normalize(Lnorm + V);
            float spec = pow(max(dot(N, H), 0.0), uMaterial.shininess);
            totalSpecular += uMaterial.specular * spec * uSpotLights[i].color * uSpotLights[i].intensity * d * atten * spotFactor;
        }
    }

    // --- FINAL COMBINATION ---
    vec3 texColor = texture(uTexture, vTex).rgb;
    
    // uAlbedo (from MeshProperties.albedo) tints the texture color.
    // This respects your request to keep the texture selector separate.
    vec3 albedoColor = texColor * uAlbedo; 
    
    // Combine ambient, diffuse (affected by albedo), and specular
    vec3 result = uMaterial.ambient + (totalDiffuse * albedoColor) + totalSpecular;
    
    FragColor = vec4(result * uMaterial.opacity, uMaterial.opacity);
}