///////////////////////////////////////////////////////////////////////////////
///
/// @file ParticleSystem.cpp
/// @brief Implementation of ParticleSystem
///
///////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "ParticleSystem.h"

#include <algorithm>
#include <cmath>

extern Framework::Coordinator ecsInterface;

namespace Framework
{
    // ------------------------------------------------------------
    // RNG
    // ------------------------------------------------------------
    uint32_t ParticleSystem::XorShift32(uint32_t& state)
    {
        // xorshift32: small, fast, decent for particles
        uint32_t x = state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state = x;
        return x;
    }

    float ParticleSystem::Rand01(uint32_t& state)
    {
        // Convert to [0,1)
        // Keep 24 bits to avoid precision weirdness
        const uint32_t r = XorShift32(state) & 0x00FFFFFFu;
        return static_cast<float>(r) / static_cast<float>(0x01000000u);
    }

    float ParticleSystem::RandSigned(uint32_t& state)
    {
        return (Rand01(state) * 2.0f) - 1.0f;
    }

    // ------------------------------------------------------------
    // System interface
    // ------------------------------------------------------------
    void ParticleSystem::Initialize()
    {
        // Register the particle component (safe even if already registered elsewhere)
        ecsInterface.RegisterComponent<ParticleEmitterComponent>();

        // Require BOTH Transform + ParticleEmitterComponent
        Signature signature;
        signature.set(ecsInterface.GetComponentType<TransformComponent>());
        signature.set(ecsInterface.GetComponentType<ParticleEmitterComponent>());
        ecsInterface.SetSystemSignature<ParticleSystem>(signature);
    }

    std::string ParticleSystem::GetName()
    {
        return "ParticleSystem";
    }

    void ParticleSystem::Update(float dt)
    {
        // Basic safety cap (prevents huge bursts on lag spikes)
        if (dt <= 0.0f) return;
        dt = std::min(dt, 0.05f); // 50ms cap

        for (Entity e : mEntities)
        {
            auto& emitter = ecsInterface.GetComponent<ParticleEmitterComponent>(e);

            if (!emitter.active)
                continue;

            // Some components in your engine have "isActive" flags; respect Transform too
            auto& xform = ecsInterface.GetComponent<TransformComponent>(e);
            if (!xform.isActive)
                continue;

            EnsurePool(emitter);
            SpawnParticles(emitter, xform, dt);
            Simulate(emitter, dt);
        }
    }

    // ------------------------------------------------------------
    // Implementation details
    // ------------------------------------------------------------
    void ParticleSystem::EnsurePool(ParticleEmitterComponent& emitter)
    {
        // Sanity clamp
        if (emitter.maxParticles < 0) emitter.maxParticles = 0;

        // Reserve to avoid realloc churn
        if (emitter.particles.capacity() < static_cast<size_t>(emitter.maxParticles))
            emitter.particles.reserve(static_cast<size_t>(emitter.maxParticles));

        // If user reduces maxParticles in inspector, trim any excess
        if (static_cast<int>(emitter.particles.size()) > emitter.maxParticles)
            emitter.particles.resize(static_cast<size_t>(emitter.maxParticles));
    }

    void ParticleSystem::SpawnParticles(ParticleEmitterComponent& emitter,
        const TransformComponent& xform,
        float dt)
    {
        if (emitter.emissionRate <= 0.0f || emitter.maxParticles <= 0)
            return;

        // Accumulate fractional emission
        emitter.emitAccumulator += emitter.emissionRate * dt;

        // Spawn as many whole particles as we have budget for
        int spawnCount = static_cast<int>(std::floor(emitter.emitAccumulator));
        if (spawnCount <= 0)
            return;

        emitter.emitAccumulator -= static_cast<float>(spawnCount);

        // Respect maxParticles capacity
        const int available = emitter.maxParticles - static_cast<int>(emitter.particles.size());
        if (available <= 0)
            return;

        spawnCount = std::min(spawnCount, available);

        // Spawn origin:
        // - world space: spawn at emitter position
        // - local space: spawn at local origin (renderer can add emitter transform later)
        const glm::vec3 origin = emitter.localSpace ? glm::vec3(0.0f) : xform.position;

        for (int i = 0; i < spawnCount; ++i)
        {
            Particle p;
            p.age = 0.0f;
            p.lifetime = std::max(0.01f, emitter.lifetime);

            // Position
            p.position = origin;

            // Velocity with jitter (component-wise)
            const glm::vec3 jitter{
                emitter.velocityJitter.x * RandSigned(emitter.rngState),
                emitter.velocityJitter.y * RandSigned(emitter.rngState),
                emitter.velocityJitter.z * RandSigned(emitter.rngState)
            };
            p.velocity = emitter.baseVelocity + jitter;

            // Size with optional jitter
            const float sj = emitter.sizeJitter * RandSigned(emitter.rngState);
            p.size = std::max(0.0001f, emitter.startSize + sj);

            emitter.particles.push_back(p);
        }
    }

    void ParticleSystem::Simulate(ParticleEmitterComponent& emitter, float dt)
    {
        if (emitter.particles.empty())
            return;

        // Integrate and remove dead using swap-pop (fast)
        for (size_t i = 0; i < emitter.particles.size(); /* increment inside */)
        {
            Particle& p = emitter.particles[i];

            p.age += dt;
            if (p.age >= p.lifetime)
            {
                // Kill (swap with back)
                emitter.particles[i] = emitter.particles.back();
                emitter.particles.pop_back();
                continue; // don't increment i (new particle at i)
            }

            // Physics integration
            p.velocity += emitter.gravity * dt;
            p.position += p.velocity * dt;

            // Size lerp from startSize -> endSize (uses normalized age)
            const float t = p.normalizedAge();
            const float sizeNow = (1.0f - t) * emitter.startSize + t * emitter.endSize;
            p.size = std::max(0.0001f, sizeNow);

            ++i;
        }
    }
}
