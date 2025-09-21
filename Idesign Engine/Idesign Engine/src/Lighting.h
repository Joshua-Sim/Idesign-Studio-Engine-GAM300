#pragma once
#include <glm.hpp>
#include "ShaderProgram.h"

enum class LightType { Directional, Point, Spot };


// CPU-side versions used in your renderer
struct DirectionalLightCPU {
    glm::vec3 direction{ -0.5f, -1.0f, -0.3f };
    glm::vec3 color{ 1.0f, 1.0f, 0.9f };
    float intensity = 1.0f;
    bool enabled = true;
};

struct PointLightCPU {
    glm::vec3 position{ 0 };
    glm::vec3 color{ 1.0f, 1.0f, 1.0f };
    float intensity = 1.0f;
    float radius = 10.0f;
    bool enabled = true; //  toggle
};

struct SpotLightCPU {
    glm::vec3 position{ 0 };
    glm::vec3 direction{ 0, -1, 0 };
    glm::vec3 color{ 1, 1, 1 };
    float intensity = 1.0f;
    float radius = 15.0f;
    float innerCutoff = glm::radians(12.5f);
    float outerCutoff = glm::radians(17.5f);
    bool enabled = true; //  toggle
};


struct LightComponent
{
    LightType type = LightType::Point;
    glm::vec3 color{ 1.0f, 1.0f, 1.0f };
    float intensity = 1.0f;

    // For point/spot
    float radius = 10.0f;

    // For spot
    float innerCutoff = glm::radians(12.5f);
    float outerCutoff = glm::radians(17.5f);

    bool enabled = true; //  allow toggling

    LightComponent() = default;

    static LightComponent CreateDirectional(const glm::vec3& color, float intensity) {
        LightComponent l;
        l.type = LightType::Directional;
        l.color = color;
        l.intensity = intensity;
        return l;
    }

    static LightComponent CreatePoint(const glm::vec3& color, float intensity, float radius) {
        LightComponent l;
        l.type = LightType::Point;
        l.color = color;
        l.intensity = intensity;
        l.radius = radius;
        return l;
    }

    static LightComponent CreateSpot(const glm::vec3& color, float intensity,
        float radius, float inner, float outer) {
        LightComponent l;
        l.type = LightType::Spot;
        l.color = color;
        l.intensity = intensity;
        l.radius = radius;
        l.innerCutoff = inner;
        l.outerCutoff = outer;
        return l;
    }

public:
    static void upload(ShaderProgram& shader,
        const DirectionalLightCPU& sun,
        bool hasSun,
        const std::vector<PointLightCPU>& pointLights,
        const std::vector<SpotLightCPU>& spotLights);
};