///////////////////////////////////////////////////////////////////////////////
///
///	@file  : LightProperties.h
/// @Brief : Properties for light
///	
///	@Authors: Rui Jie
///	@Copyright 2025, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////

#pragma once
#ifndef _LIGHT_PROPERTIES_H_
#define _LIGHT_PROPERTIES_H_

#include "PropertySystem.h"
#include "PropertyMacro.h"
#include <glm.hpp>
#include "ShaderProgram.h"
#include <string>  
#include <vector> 
#include <cmath>   

namespace Framework
{
    // ========================================================================
    // CPU-SIDE (SHADER-FACING) STRUCTS
    // Moved outside LightProperties to be helper structs
    // ========================================================================

    struct DirectionalLightCPU {
        glm::vec3 direction{ 0.0f, -0.707f, -0.707f };
        float intensity{ 1.0f };
        glm::vec3 color{ 1.0f, 1.0f, 1.0f };
    };

    struct PointLightCPU {
        glm::vec3 position{ 0.f, 0.f, 0.f };
        glm::vec3 color{ 1.f, 1.f, 1.f };
        float intensity{ 1.0f };
        float radius{ 10.0f };
        bool enabled{ true };
    };

    struct SpotLightCPU {
        glm::vec3 position{ 0.f, 0.f, 0.f };
        glm::vec3 direction{ 0.f, -1.f, 0.f };
        glm::vec3 color{ 1.f, 1.f, 1.f };
        float intensity{ 1.0f };
        float radius{ 12.0f };
        // Store the angles in radians, not cosines, to match the component
        float innerCutOffAngle{ glm::radians(12.5f) };
        float outerCutOffAngle{ glm::radians(15.0f) };
        bool enabled{ true };
    };

    // ========================================================================
    // LIGHT COMPONENT DEFINITION
    // ========================================================================

    // Defines the type of light
    enum class LightType
    {
        Directional,
        Point,
        Spot
    };

    struct LightProperties
    {
        LightType type = LightType::Point;

        // --- Serialized Properties ---
        bool enabled = true;
        glm::vec3 color = { 1.0f, 1.0f, 1.0f };
        float intensity = 1.0f;
        float radius = 10.0f;
        float innerCutOffAngle = glm::radians(12.5f);
        float outerCutOffAngle = glm::radians(15.0f);



        static void upload(ShaderProgram& shader,
            const DirectionalLightCPU& sun,
            bool hasSun,
            const std::vector<PointLightCPU>& pointLights,
            const std::vector<SpotLightCPU>& spotLights)
        {
            // --- Directional ---
            shader.setBool("uHasDirLight", hasSun);
            if (hasSun) {
                shader.setVec3("uDirLight.direction", sun.direction);
                shader.setVec3("uDirLight.color", sun.color);
                shader.setFloat("uDirLight.intensity", sun.intensity);
            }

            // --- Point lights ---
            int activePoints = 0;
            for (int i = 0; i < (int)pointLights.size(); ++i) {
                // Check the 'enabled' flag from the CPU struct
                if (!pointLights[i].enabled) continue;
                if (activePoints >= 8) break; // Hardcoded max lights for safety

                std::string base = "uPointLights[" + std::to_string(activePoints) + "]";
                shader.setVec3((base + ".position").c_str(), pointLights[i].position);
                shader.setVec3((base + ".color").c_str(), pointLights[i].color);
                shader.setFloat((base + ".intensity").c_str(), pointLights[i].intensity);
                shader.setFloat((base + ".radius").c_str(), pointLights[i].radius);

                activePoints++;
            }
            shader.setInt("uNumPointLights", activePoints);

            // --- Spot lights ---
            int activeSpots = 0;
            for (int i = 0; i < (int)spotLights.size(); ++i) {
                if (!spotLights[i].enabled) continue;
                if (activeSpots >= 8) break; // Hardcoded max lights for safety

                std::string base = "uSpotLights[" + std::to_string(activeSpots) + "]";
                shader.setVec3((base + ".position").c_str(), spotLights[i].position);
                shader.setVec3((base + ".direction").c_str(), spotLights[i].direction);
                shader.setVec3((base + ".color").c_str(), spotLights[i].color);
                shader.setFloat((base + ".intensity").c_str(), spotLights[i].intensity);
                shader.setFloat((base + ".radius").c_str(), spotLights[i].radius);
                // Send the cosine of the angles, which the shader expects
                shader.setFloat((base + ".innerCutoff").c_str(), std::cos(spotLights[i].innerCutOffAngle));
                shader.setFloat((base + ".outerCutoff").c_str(), std::cos(spotLights[i].outerCutOffAngle));

                activeSpots++;
            }
            shader.setInt("uNumSpotLights", activeSpots);
        }
    };

    // Creates the property list for the editor
    inline const std::vector g_LightPropsArray =
    {
        PROP_BOOL(enabled, LightProperties),
        PROP_COLOR3(color, LightProperties),
        PROP_SLIDER_FLOAT(intensity, LightProperties, 0.0f, 20.0f),
        PROP_DRAG_FLOAT(radius, LightProperties),
        PROP_SLIDER_FLOAT(innerCutOffAngle, LightProperties, 0.0f, 1.5708f), // 0-90 deg
        PROP_SLIDER_FLOAT(outerCutOffAngle, LightProperties, 0.0f, 1.5708f)	 // 0-90 deg
    };

    inline const prop_list g_LightProps{ "LightProperties", g_LightPropsArray };

    // Global registration instance
    inline global_registration g_LightPropsReg(&g_LightProps);
}

#endif // !_LIGHT_PROPERTIES_H_