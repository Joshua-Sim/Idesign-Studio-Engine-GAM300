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
        glm::vec3 position{ 0.f, 0.f, 0.f };
        float shadowBiasConst{ 0.005f };
        float shadowBiasFactor{ 0.05f };
    };

    struct PointLightCPU {
        glm::vec3 position{ 0.f, 0.f, 0.f };
        glm::vec3 color{ 1.f, 1.f, 1.f };
        glm::vec3 offset{ 0.0f, 0.0f, 0.0f };
        float intensity{ 1.0f };
        float radius{ 10.0f };
        bool enabled{ true };
        bool castsShadow{ false };
        // ACNE FIX: shadowBiasConst is the flat floor bias (world-space units).
        // shadowBiasFactor scales with surface slope in the shader.
        // 0.005 was too small for interior scenes — bumped to safer defaults.
        // Tune per-light via the editor if Peter-Panning appears.
        float shadowBiasConst{ 0.08f };
        float shadowBiasFactor{ 0.04f };

        // Carry the actual far plane used when rendering the cubemap
        float shadowFarPlane{ 100.0f };
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
        glm::vec3 offset{ 0.0f, 0.0f, 0.0f };
        glm::vec3 color = { 1.0f, 1.0f, 1.0f };
        float intensity = 1.0f;
        float radius = 10.0f;
        float innerCutOffAngle = glm::radians(12.5f);
        float outerCutOffAngle = glm::radians(15.0f);
        bool castsShadow = false;
        float shadowBiasConst{ 0.08f };
        float shadowBiasFactor{ 0.04f };
        // --- Shadow Resources ---
        // share the FBO ID for all light types)
        GLuint    shadowFBO = 0;

        // For Dir/Spot lights (sampler2D)
        GLuint    shadowDepthTexture = 0;
        glm::mat4 lightSpaceMatrix;

        // For Point lights (samplerCube)
        GLuint    shadowDepthCubeMap = 0; // The 6-sided texture
        GLuint    shadowDepthRBO = 0;     // Depth buffer for the FBO
        float     shadowFarPlane = 100.0f;  // The far plane used when the cubemap was rendered


        static void upload(ShaderProgram& shader,
            const DirectionalLightCPU& sun,
            bool hasSun,
            const std::vector<PointLightCPU>& pointLights,
            const std::vector<SpotLightCPU>& spotLights)
        {
            // 1. Static cache to hold uniform names permanently in memory
            static std::vector<std::string> p_pos, p_col, p_int, p_rad, p_cast, p_bConst, p_bFact, p_far;
            static std::vector<std::string> s_pos, s_dir, s_col, s_int, s_rad, s_in, s_out;
            static bool initialized = false;

            // 2. Build the strings ONLY ONCE on the very first call
            if (!initialized) {
                for (int i = 0; i < 8; ++i) { // 8 is your hardcoded max
                    std::string pBase = "uPointLights[" + std::to_string(i) + "]";
                    p_pos.push_back(pBase + ".position");
                    p_col.push_back(pBase + ".color");
                    p_int.push_back(pBase + ".intensity");
                    p_rad.push_back(pBase + ".radius");
                    p_cast.push_back(pBase + ".castsShadow");
                    p_bConst.push_back(pBase + ".shadowBiasConst");
                    p_bFact.push_back(pBase + ".shadowBiasFactor");
                    p_far.push_back(pBase + ".shadowFarPlane");

                    std::string sBase = "uSpotLights[" + std::to_string(i) + "]";
                    s_pos.push_back(sBase + ".position");
                    s_dir.push_back(sBase + ".direction");
                    s_col.push_back(sBase + ".color");
                    s_int.push_back(sBase + ".intensity");
                    s_rad.push_back(sBase + ".radius");
                    s_in.push_back(sBase + ".innerCutoff");
                    s_out.push_back(sBase + ".outerCutoff");
                }
                initialized = true;
            }

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
                if (!pointLights[i].enabled) continue;
                if (activePoints >= 8) break;

                // 3. Use the cached strings via .c_str() (Zero allocations!)
                shader.setVec3(p_pos[activePoints].c_str(), pointLights[i].position);
                shader.setVec3(p_col[activePoints].c_str(), pointLights[i].color);
                shader.setVec3(p_pos[activePoints].c_str(), pointLights[i].position + pointLights[i].offset);
                shader.setFloat(p_int[activePoints].c_str(), pointLights[i].intensity);
                shader.setFloat(p_rad[activePoints].c_str(), pointLights[i].radius);
                shader.setBool(p_cast[activePoints].c_str(), pointLights[i].castsShadow);
                shader.setFloat(p_bConst[activePoints].c_str(), pointLights[i].shadowBiasConst);
                shader.setFloat(p_bFact[activePoints].c_str(), pointLights[i].shadowBiasFactor);
                shader.setFloat(p_far[activePoints].c_str(), pointLights[i].shadowFarPlane);
                activePoints++;
            }
            shader.setInt("uNumPointLights", activePoints);

            // --- Spot lights ---
            int activeSpots = 0;
            for (int i = 0; i < (int)spotLights.size(); ++i) {
                if (!spotLights[i].enabled) continue;
                if (activeSpots >= 8) break;

                // 4. Use the cached strings here as well
                shader.setVec3(s_pos[activeSpots].c_str(), spotLights[i].position);
                shader.setVec3(s_dir[activeSpots].c_str(), spotLights[i].direction);
                shader.setVec3(s_col[activeSpots].c_str(), spotLights[i].color);
                shader.setFloat(s_int[activeSpots].c_str(), spotLights[i].intensity);
                shader.setFloat(s_rad[activeSpots].c_str(), spotLights[i].radius);
                shader.setFloat(s_in[activeSpots].c_str(), std::cos(spotLights[i].innerCutOffAngle));
                shader.setFloat(s_out[activeSpots].c_str(), std::cos(spotLights[i].outerCutOffAngle));
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
        PROP_VEC3(offset, LightProperties),
        PROP_SLIDER_FLOAT(innerCutOffAngle, LightProperties, 0.0f, 1.5708f), // 0-90 deg
        PROP_SLIDER_FLOAT(outerCutOffAngle, LightProperties, 0.0f, 1.5708f), // 0-90 deg
        PROP_BOOL(castsShadow, LightProperties),
        PROP_DRAG_FLOAT(shadowBiasConst, LightProperties, 0.0f, 0.01f),
        PROP_DRAG_FLOAT(shadowBiasFactor, LightProperties, 0.0f, 0.1f),
        PROP_DRAG_FLOAT(shadowFarPlane, LightProperties)
    };

    inline const prop_list g_LightProps{ "LightProperties", g_LightPropsArray };

    // Global registration instance
    inline global_registration g_LightPropsReg(&g_LightProps);
}

#endif // !_LIGHT_PROPERTIES_H_