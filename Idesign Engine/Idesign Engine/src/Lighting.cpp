#include "pch.h"
#include "Lighting.h"
#include "ShaderProgram.h"
#include <string>
#include <cmath>


void LightComponent::upload(ShaderProgram& shader,
    const DirectionalLightCPU& sun,
    bool hasSun,
    const std::vector<PointLightCPU>& pointLights,
    const std::vector<SpotLightCPU>& spotLights) {
    // Directional
    shader.setBool("uHasDirLight", hasSun);
    if (hasSun && sun.enabled) {
        shader.setVec3("uDirLight.direction", sun.direction);
        shader.setVec3("uDirLight.color", sun.color);
        shader.setFloat("uDirLight.intensity", sun.intensity);
    }

    // Point lights
    int activePoints = 0;
    for (int i = 0; i < (int)pointLights.size(); ++i) {
        const auto& l = pointLights[i];
        if (!l.enabled) continue;

        std::string base = "uPointLights[" + std::to_string(activePoints) + "]";
        shader.setVec3((base + ".position").c_str(), l.position);
        shader.setVec3((base + ".color").c_str(), l.color);
        shader.setFloat((base + ".intensity").c_str(), l.intensity);
        shader.setFloat((base + ".radius").c_str(), l.radius);
        activePoints++;
    }
    shader.setInt("uNumPointLights", activePoints);

    // Spot lights
    int activeSpots = 0;
    for (int i = 0; i < (int)spotLights.size(); ++i) {
        const auto& l = spotLights[i];
        if (!l.enabled) continue;

        std::string base = "uSpotLights[" + std::to_string(activeSpots) + "]";
        shader.setVec3((base + ".position").c_str(), l.position);
        shader.setVec3((base + ".direction").c_str(), l.direction);
        shader.setVec3((base + ".color").c_str(), l.color);
        shader.setFloat((base + ".intensity").c_str(), l.intensity);
        shader.setFloat((base + ".radius").c_str(), l.radius);
        shader.setFloat((base + ".innerCutoff").c_str(), std::cos(l.innerCutoff));
        shader.setFloat((base + ".outerCutoff").c_str(), std::cos(l.outerCutoff));
        activeSpots++;
    }
    shader.setInt("uNumSpotLights", activeSpots);
}