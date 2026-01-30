///////////////////////////////////////////////////////////////////////////////
///
/// @file ParticleComponent.h
/// @brief Component for particle emitters (voxel / pixel style)
///
/// Notes:
/// - This follows your engine’s existing component + PropertyMacro pattern
///   (see AudioComponent.h / TextureProperties.h for reference).
/// - Because the property system in your repo doesn’t expose enums directly,
///   `style` is an int:
///       0 = Pixel (billboard quad)
///       1 = Voxel (cube)
///
///////////////////////////////////////////////////////////////////////////////

#pragma once
#ifndef PARTICLE_COMPONENT_H
#define PARTICLE_COMPONENT_H

#include <vector>
#include <cstdint>
#include <glm.hpp>
#include "PropertyMacro.h"   // includes PropertySystem.h + AssetHandler.h

namespace Framework
{
    // You can use this enum internally in code if you want,
    // but we store style as int for inspector compatibility.
    enum class ParticleStyle : int
    {
        Pixel = 0,
        Voxel = 1
    };

    // Runtime-only particle data (NOT exposed to inspector)
    struct Particle
    {
        glm::vec3 position{ 0.0f };
        glm::vec3 velocity{ 0.0f };
        float age = 0.0f;
        float lifetime = 1.0f;
        float size = 1.0f;

        bool alive() const { return age < lifetime; }
        float normalizedAge() const { return (lifetime > 0.0001f) ? (age / lifetime) : 1.0f; }
    };

    // Particle emitter component (attach this to an entity)
    struct ParticleEmitterComponent
    {
        // --- Inspector / serialized-ish config ---
        bool active = true;

        // Capacity & emission
        int maxParticles = 512;          // maximum particles in the pool
        float emissionRate = 60.0f;      // particles per second
        float lifetime = 1.0f;           // seconds per particle

        // Motion
        glm::vec3 baseVelocity{ 0.0f, 5.0f, 0.0f };
        glm::vec3 velocityJitter{ 1.0f, 1.0f, 1.0f }; // random +/- applied per spawn
        glm::vec3 gravity{ 0.0f, -9.81f, 0.0f };

        // Size
        float startSize = 0.15f;
        float endSize = 0.05f;
        float sizeJitter = 0.0f;

        // Style: 0 = Pixel (billboard), 1 = Voxel (cube)
        int style = static_cast<int>(ParticleStyle::Pixel);

        // If true, particles follow emitter transform (spawn & simulate in local space)
        // If false, particles are in world space once spawned
        bool localSpace = false;

        // --- Runtime-only ---
        float emitAccumulator = 0.0f;    // accumulates fractional emission
        uint32_t rngState = 0x12345678u; // simple per-emitter RNG seed/state
        std::vector<Particle> particles; // particle pool

        ParticleStyle GetStyle() const
        {
            return (style == static_cast<int>(ParticleStyle::Voxel))
                ? ParticleStyle::Voxel
                : ParticleStyle::Pixel;
        }
    };

    // Property registration for ParticleEmitterComponent
    inline const std::vector g_ParticleEmitterPropsArray =
    {
        PROP_BOOL(active,            ParticleEmitterComponent),

        PROP_INT(maxParticles,       ParticleEmitterComponent),
        PROP_FLOAT(emissionRate,     ParticleEmitterComponent),
        PROP_FLOAT(lifetime,         ParticleEmitterComponent),

        PROP_VEC3(baseVelocity,      ParticleEmitterComponent),
        PROP_VEC3(velocityJitter,    ParticleEmitterComponent),
        PROP_VEC3(gravity,           ParticleEmitterComponent),

        PROP_FLOAT(startSize,        ParticleEmitterComponent),
        PROP_FLOAT(endSize,          ParticleEmitterComponent),
        PROP_FLOAT(sizeJitter,       ParticleEmitterComponent),

        // 0 = Pixel, 1 = Voxel
        PROP_INT(style,              ParticleEmitterComponent),

        PROP_BOOL(localSpace,        ParticleEmitterComponent),
    };

    inline const prop_list g_ParticleEmitterProps{ "ParticleEmitterComponent", g_ParticleEmitterPropsArray };
    inline global_registration g_ParticleEmitterReg(&g_ParticleEmitterProps);
}

#endif // PARTICLE_COMPONENT_H
