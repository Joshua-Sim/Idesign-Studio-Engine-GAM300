///////////////////////////////////////////////////////////////////////////////
///
/// @file ParticleSystem.h
/// @brief ECS system that updates particle emitters (spawn + simulate + kill)
///
/// Integrates with your ECS Coordinator + ISystem patterns.
///
/// Requires:
/// - TransformComponent (TransformProperties.h)
/// - ParticleEmitterComponent (ParticleComponent.h)
///
///////////////////////////////////////////////////////////////////////////////

#pragma once
#ifndef PARTICLE_SYSTEM_H
#define PARTICLE_SYSTEM_H

#include "pch.h"
#include "System.h"
#include "Coordinator.h"

#include "TransformProperties.h"
#include "ParticleComponent.h"

namespace Framework
{
    class ParticleSystem : public ISystem
    {
    public:
        void Initialize() override;
        void Update(float dt) override;
        std::string GetName() override;

    private:
        // RNG helpers (fast + deterministic per-emitter)
        static uint32_t XorShift32(uint32_t& state);
        static float    Rand01(uint32_t& state);      // [0, 1)
        static float    RandSigned(uint32_t& state);  // [-1, 1)

        void EnsurePool(ParticleEmitterComponent& emitter);
        void SpawnParticles(ParticleEmitterComponent& emitter, const TransformComponent& xform, float dt);
        void Simulate(ParticleEmitterComponent& emitter, float dt);
    };
}

#endif // PARTICLE_SYSTEM_H
