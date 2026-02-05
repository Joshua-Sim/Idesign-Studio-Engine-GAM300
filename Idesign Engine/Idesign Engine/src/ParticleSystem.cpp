#include "pch.h"
#include "ParticleSystem.h"

#include <random>
#include <algorithm>
#include <glm.hpp>
#include <gtc/constants.hpp>

#include "Coordinator.h"
#include "TransformProperties.h"
#include "ParticleComponent.h"
#include "AssetManager.h"
#include "TextureProperties.h"
#include "TextureLoader.h"
#include <glew.h>

extern Framework::Coordinator ecsInterface;

namespace Framework
{
    static std::mt19937& RNG()
    {
        static std::mt19937 rng{ std::random_device{}() };
        return rng;
    }

    static float Rand01()
    {
        static std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        return dist(RNG());
    }

    // Sample a unit vector inside a cone around 'dir' (dir must be normalized)
    static glm::vec3 SampleCone(const glm::vec3& dir, float coneAngleRad)
    {
        // Build orthonormal basis (tangent, bitangent, dir)
        glm::vec3 w = dir;
        glm::vec3 a = (std::abs(w.y) < 0.99f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
        glm::vec3 u = glm::normalize(glm::cross(a, w));
        glm::vec3 v = glm::cross(w, u);

        // Uniform over cone solid angle
        float cosMax = std::cos(coneAngleRad);
        float z = cosMax + (1.0f - cosMax) * Rand01();
        float phi = 2.0f * glm::pi<float>() * Rand01();
        float r = std::sqrt(std::max(0.0f, 1.0f - z * z));

        float x = r * std::cos(phi);
        float y = r * std::sin(phi);

        return glm::normalize(u * x + v * y + w * z);
    }

    static void EnsureEmitterTextureLoaded(ParticleEmitterComponent& em)
    {
        if (!em.textureGuid) { em.textureID = 0; return; }

        auto* tex = AssetManager::Instance().Get<TextureProperties>(em.textureGuid);
        if (!tex) { em.textureID = 0; return; }

        if (tex->textureID == 0 && !tex->path.empty())
        {
            TextureLoader::loadTexture(*tex, true);
        }

        em.textureID = tex->textureID;

        // Pixel look: force nearest filtering
        if (em.pixelFilter && em.textureID != 0)
        {
            glBindTexture(GL_TEXTURE_2D, em.textureID);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    void ParticleSystem::Initialize()
    {
        Signature sig;
        sig.set(ecsInterface.GetComponentType<ParticleEmitterComponent>());
        sig.set(ecsInterface.GetComponentType<TransformComponent>());
        ecsInterface.SetSystemSignature<ParticleSystem>(sig);
    }

    void ParticleSystem::Update(float dt)
    {
        if (dt > 0.1f) 
            dt = 0.016f;

        for (Entity e : mEntities)
        {
            if (ecsInterface.HasComponent<ParticleEmitterComponent>(e) &&
                ecsInterface.HasComponent<TransformComponent>(e))
            {
                auto& em = ecsInterface.GetComponent<ParticleEmitterComponent>(e);
                auto& tr = ecsInterface.GetComponent<TransformComponent>(e);

                // Always update existing particles (even if emitter disabled)
                // but only spawn if active + transform active
                bool canEmit = true;
                if (!em.loop)
                {
                    em.timeSinceStart += dt;
                    if (em.timeSinceStart > em.duration)
                        canEmit = false;
                }

                EnsureEmitterTextureLoaded(em);
            
            // --- Emission timing (loop vs duration) ---
            if (canEmit)
            {
                em.timeSinceStart += dt;

                if (!em.loop && em.timeSinceStart > em.duration)
                {
                    canEmit = false; // stop emitting, but keep updating remaining particles
                }
            }

            // --- Spawn ---
            if (canEmit && em.particlesPerSecond > 0.0f && em.particles.size() < (size_t)em.maxParticles)
            {
                em.emitAccumulator += dt * em.particlesPerSecond;

                const float spreadRad = glm::radians(std::max(0.0f, em.spreadAngleDeg));
                glm::vec3 baseDir = em.direction;
                if (glm::dot(baseDir, baseDir) < 1e-6f)
                    baseDir = glm::vec3(0, 1, 0);

                while (em.emitAccumulator >= 1.0f && em.particles.size() < (size_t)em.maxParticles)
                {
                    em.emitAccumulator -= 1.0f;

                    Particle p;
                    p.position = glm::vec3(0.0f); // local; Graphics.cpp already supports localSpace
                    glm::vec3 dir = SampleCone(baseDir, spreadRad);

                    float speed = std::max(0.0f, em.particleSpeed);
                    p.velocity = dir * speed;

                    // Derive lifetime from requested range + speed
                    float denom = std::max(0.01f, speed);
                    p.lifetime = std::max(0.05f, em.range / denom);

                    p.size = std::max(0.001f, em.particleSize);
                    p.age = 0.0f;

                    em.particles.push_back(p);
                }
            }

            // --- Simulate + kill ---
            for (size_t i = 0; i < em.particles.size();)
            {
                Particle& p = em.particles[i];
                p.age += dt;
                p.position += p.velocity * dt;

                if (p.age >= p.lifetime)
                {
                    em.particles[i] = em.particles.back();
                    em.particles.pop_back();
                    continue;
                }
                ++i;
            }

            // If non-loop and finished, keep time clamped so it doesn't grow forever
            if (!em.loop && em.timeSinceStart > em.duration + 10.0f)
                em.timeSinceStart = em.duration + 10.0f;
            }
        }
    }
}
